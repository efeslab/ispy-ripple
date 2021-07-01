#ifndef LBR_MISS_SAMPLE_H_
#define LBR_MISS_SAMPLE_H_

#include <bits/stdc++.h>
#include <chrono>

using namespace std;

#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <boost/dynamic_bitset.hpp>

#include "gz_reader.h"
#include "log.h"
#include "convert.h"
#include "cfg.h"
#include "suffixtree.h"
#include "classifier.h"

int measure_prefetch_length(uint64_t current, uint64_t next)
{
    return (next >> 6) - (current >> 6);
}

int measure_prefetch_length(pair<uint64_t, uint64_t> &current, pair<uint64_t, uint64_t> &next)
{
    return next.first - current.first;
}

unsigned int my_abs(int a)
{
    if (a < 0)
        return -a;
    return a;
}

class ContextCandidate
{
public:
    uint64_t index;
    uint64_t context_size;
    uint64_t count;
    ContextCandidate(uint64_t i, uint64_t c_size, uint64_t c) : index(i), context_size(c_size), count(c) {}
    bool operator<(const ContextCandidate &right) const
    {
        if (count == right.count)
        {
            //Context bigger is smaller
            if (context_size == right.context_size)
            {
                //index bigger is smaller
                return index > right.index;
            }
            else
            {
                return context_size > right.context_size;
            }
        }
        else
        {
            return count < right.count;
        }
    }
};

class Candidate
{
public:
    uint64_t predecessor_bbl_address;
    double covered_miss_ratio_to_predecessor_count;
    Candidate(uint64_t addr, double c_ratio)
    {
        predecessor_bbl_address = addr;
        covered_miss_ratio_to_predecessor_count = c_ratio;
    }
    bool operator<(const Candidate &right) const
    {
        return covered_miss_ratio_to_predecessor_count < right.covered_miss_ratio_to_predecessor_count;
    }
};

struct prefetch_benefit
{
    uint64_t prefetch_count;
    uint64_t covered_miss_count;
    uint64_t cache_line_id;
    uint64_t bbl_address;
    prefetch_benefit(uint64_t p_count, uint64_t cm_count, uint64_t cl_id, uint64_t b_addr)
    {
        prefetch_count = p_count;
        covered_miss_count = cm_count;
        cache_line_id = cl_id;
        bbl_address = b_addr;
    }
    bool operator<(const prefetch_benefit &right) const
    {
        if (prefetch_count < 1)
            return false;
        if (right.prefetch_count < 1)
            return true;
        double left_ratio = (1.0 * covered_miss_count) / prefetch_count;
        double right_ratio = (1.0 * right.covered_miss_count) / right.prefetch_count;
        if (left_ratio == right_ratio)
        {
            if (covered_miss_count == right.covered_miss_count)
            {
                return prefetch_count < right.prefetch_count;
            }
            else
            {
                return covered_miss_count < right.covered_miss_count;
            }
        }
        else
        {
            return left_ratio < right_ratio;
        }
    }
};

struct benefit
{
    uint64_t missed_bbl_address;
    uint64_t cache_line_id;
    uint64_t predecessor_bbl_address;
    uint64_t covered_miss_counts;
    double fan_in;
    benefit(uint64_t m, uint64_t cl_id, uint64_t p, uint64_t c, double f) : missed_bbl_address(m), cache_line_id(cl_id), predecessor_bbl_address(p), covered_miss_counts(c), fan_in(f) {}
    benefit(const benefit &b)
    {
        missed_bbl_address = b.missed_bbl_address;
        cache_line_id = b.cache_line_id;
        predecessor_bbl_address = b.predecessor_bbl_address;
        covered_miss_counts = b.covered_miss_counts;
        fan_in = b.fan_in;
    }
    bool operator<(const benefit &right) const
    {
        if (right.fan_in > fan_in)
            return true;
        else if (fan_in > right.fan_in)
            return false;
        else
        {
            if (right.covered_miss_counts > covered_miss_counts)
                return true;
            return false;
        }
    }
};

struct multi_bbl_benefit
{
    uint64_t missed_bbl_address;
    uint64_t cache_line_id;
    uint64_t predecessor_bbl_address;
    uint64_t predicate_bbl_address;
    uint64_t covered_miss_counts;
    double fan_in;
    uint64_t dynamic_prefetch_counts;
    multi_bbl_benefit(uint64_t m, uint64_t cl_id, uint64_t predecessor, uint64_t predicate, uint64_t c, double f, uint64_t d_p_counts) : missed_bbl_address(m), cache_line_id(cl_id), predecessor_bbl_address(predecessor), predicate_bbl_address(predicate), covered_miss_counts(c), fan_in(f), dynamic_prefetch_counts(d_p_counts) {}
    bool operator<(const multi_bbl_benefit &right) const
    {
        if (right.fan_in > fan_in)
            return true;
        else if (fan_in > right.fan_in)
            return false;
        else
        {
            if (right.covered_miss_counts > covered_miss_counts)
                return true;
            return false;
        }
    }
};

class LBR_Sample
{
public:
    vector<pair<uint64_t, uint64_t>> current_lbr;
    uint64_t current_bbl_index;
    LBR_Sample()
    {
        current_lbr.clear();
        current_bbl_index = 0;
    }
    void set_current_bbl_index(uint64_t i)
    {
        current_bbl_index = i;
    }
    void push_pair(uint64_t prev_bbl, uint64_t cycle)
    {
        current_lbr.push_back(make_pair(prev_bbl, cycle));
    }
    void push(string lbr_pair)
    {
        vector<string> current_record;
        boost::split(current_record, lbr_pair, boost::is_any_of(";"), boost::token_compress_on);
        if (current_record.size() != 2)
            panic("LB-record does not contain exactly 2 entries\n");
        push_pair(string_to_u64(current_record[0]), string_to_u64(current_record[1]));
    }
    uint64_t size()
    {
        return current_lbr.size();
    }
    void get_string(vector<uint64_t> &strings, Settings *settings, CFG *dynamic_cfg)
    {
        strings.clear();
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 0; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                strings.push_back(current_lbr[i].first);
                if (current_distance > max_distance)
                    return;
            }
        }
        reverse(strings.begin(), strings.end());
    }
    void get_candidates(unordered_map<uint64_t, uint64_t> &candidates, Settings *settings, CFG *dynamic_cfg)
    {
        candidates.clear();
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 1; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                uint64_t predecessor = current_lbr[i].first;
                if (candidates.find(predecessor) == candidates.end())
                {
                    candidates[predecessor] = current_bbl_index - i;
                }
                if (current_distance > max_distance)
                    return;
            }
        }
    }
    void get_candidates(set<uint64_t> &candidates, Settings *settings, CFG *dynamic_cfg)
    {
        candidates.clear();
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 1; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                candidates.insert(current_lbr[i].first);
                if (current_distance > max_distance)
                    return;
            }
        }
    }
    void get_correlated_candidates(uint64_t top_candidate, set<uint64_t> &candidates, Settings *settings, CFG *dynamic_cfg)
    {
        candidates.clear();
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 0; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                if (current_lbr[i].first == top_candidate)
                {
                    //insert previous 8 BBLs on the candidate list
                    for (uint64_t j = i + 1; (j < current_lbr.size() && j < i + 9); j++)
                    {
                        if (current_lbr[j].first == top_candidate)
                            continue;
                        candidates.insert(current_lbr[j].first);
                    }
                    return;
                }
                if (current_distance > max_distance)
                    return;
            }
        }
    }
    bool is_covered(uint64_t predecessor_bbl_address, Settings *settings, CFG *dynamic_cfg)
    {
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 0; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                if (current_lbr[i].first == predecessor_bbl_address)
                    return true;
                if (current_distance > max_distance)
                    break;
            }
        }
        return false;
    }
    bool is_covered_with_predicate(uint64_t predecessor_bbl_address, uint64_t predicate, Settings *settings, CFG *dynamic_cfg)
    {
        uint64_t current_distance = 0;

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        for (uint64_t i = 0; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                if (current_lbr[i].first == predecessor_bbl_address)
                {
                    //check previous 8 BBLs for the predicate
                    for (uint64_t j = i + 1; (j < current_lbr.size() && j < i + 9); j++)
                    {
                        if (current_lbr[j].first == predicate)
                            return true;
                    }
                }
                if (current_distance > max_distance)
                    break;
            }
        }
        return false;
    }
    uint64_t count_accessed_cache_lines(Settings *settings, CFG *dynamic_cfg)
    {

        uint64_t min_distance = settings->get_min_distance();
        uint64_t max_distance = settings->get_max_distance();

        set<uint64_t> unique_cache_lines;
        uint64_t current_distance = 0;

        for (uint64_t i = 0; i < current_lbr.size(); i++)
        {
            if (settings->multiline_mode == 1 || settings->remove_lbr_cycle == true) //ASMDB
            {
                current_distance += dynamic_cfg->get_bbl_instr_count(current_lbr[i].first);
            }
            else // if(settings.multiline_mode == 2) OUR
            {
                current_distance += dynamic_cfg->get_bbl_avg_cycles(current_lbr[i].first); //current_lbr[i].second;
            }
            if (current_distance >= min_distance)
            {
                uint64_t start = current_lbr[i].first;
                uint64_t end = start + dynamic_cfg->get_bbl_size(start);
                start >>= 6;
                end >>= 6;
                for (uint64_t j = start; j <= end; j++)
                    unique_cache_lines.insert(j);
                if (current_distance > max_distance)
                    break;
            }
        }

        return unique_cache_lines.size();
    }
    void clear()
    {
        current_lbr.clear();
    }
};

class LBR_List
{
public:
    vector<LBR_Sample *> all_misses;
    LBR_List()
    {
        all_misses.clear();
    }
    void push_miss_sample(LBR_Sample *current_sample)
    {
        all_misses.push_back(current_sample);
    }
    void push(vector<string> lbr_pairs)
    {
        LBR_Sample *current_sample = new LBR_Sample();
        for (uint64_t i = 2 /*omit first two items, cache line id and missed bbl address*/; i < lbr_pairs.size(); i++)
        {
            //current_sample->push(lbr_pairs[i]);
            vector<string> current_record;
            boost::split(current_record, lbr_pairs[i], boost::is_any_of(";"), boost::token_compress_on);
            if (current_record.size() == 1)
            {
                current_sample->set_current_bbl_index(string_to_u64(current_record[0]));
            }
            else if (current_record.size() == 2)
            {
                current_sample->push_pair(string_to_u64(current_record[0]), string_to_u64(current_record[1]));
            }
            else
            {
                panic("LB-record does not contain exactly 1 or 2 entries\n");
            }

            current_record.clear();
        }
        push_miss_sample(current_sample);
    }
    uint64_t size()
    {
        return all_misses.size();
    }
    void measure_prefetch_window_cdf(unordered_map<uint64_t, uint64_t> &table, Settings *settings, CFG *dynamic_cfg)
    {
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            uint64_t accessed_cache_line_count = all_misses[i]->count_accessed_cache_lines(settings, dynamic_cfg);
            if (table.find(accessed_cache_line_count) == table.end())
                table[accessed_cache_line_count] = 1;
            else
                table[accessed_cache_line_count] += 1;
        }
    }
    void get_suffixes(vector<vector<uint64_t>> &common_patterns, vector<boost::dynamic_bitset<>> &bitmaps, Settings *settings, CFG *dynamic_cfg)
    {
        vector<vector<uint64_t>> all_miss_strings;
        SuffixTree<uint64_t> stree;
        uint64_t total_miss_count = all_misses.size();
        for (uint64_t i = 0; i < total_miss_count; i++)
        {
            vector<uint64_t> current_miss_string;
            all_misses[i]->get_string(current_miss_string, settings, dynamic_cfg);
            current_miss_string.push_back(all_miss_strings.size());
            all_miss_strings.push_back(current_miss_string);
            stree.add_string(current_miss_string.begin(), current_miss_string.end());
        }
        vector<vector<uint64_t>> tmp_common_patterns;
        vector<boost::dynamic_bitset<>> tmp_bitmaps;
        stree.dfs_visit(tmp_common_patterns, tmp_bitmaps);
        for (uint64_t i = 0; i < tmp_common_patterns.size(); i++)
        {
            //if(tmp_common_patterns[i].size()>16)continue;
            bool has_self_modifying = false;
            for (auto it : tmp_common_patterns[i])
            {
                if (dynamic_cfg->is_self_modifying(it))
                {
                    has_self_modifying = true;
                    break;
                }
            }
            if (has_self_modifying)
                continue;
            common_patterns.push_back(vector<uint64_t>(tmp_common_patterns[i]));
            bitmaps.push_back(boost::dynamic_bitset<>(tmp_bitmaps[i]));
        }
        for (uint64_t i = 0; i < tmp_common_patterns.size(); i++)
        {
            tmp_common_patterns[i].clear();
        }
        tmp_common_patterns.clear();
        tmp_bitmaps.clear();

        for (auto it : all_miss_strings)
        {
            it.clear();
        }
        all_miss_strings.clear();
    }
    bool get_next_candidate(uint64_t *result, vector<uint64_t> &result_miss_index, Settings *settings, set<uint64_t> &omit_list, CFG *dynamic_cfg, double *fan_out_ratio = nullptr)
    {
        unordered_map<uint64_t, vector<uint64_t>> candidate_counts;
        unordered_map<uint64_t, uint64_t> candidates;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            all_misses[i]->get_candidates(candidates, settings, dynamic_cfg);
            for (auto it : candidates)
            {
                uint64_t predecessor = it.first;
                uint64_t index = it.second;
                if (omit_list.find(predecessor) != omit_list.end())
                    continue;
                if (candidate_counts.find(predecessor) == candidate_counts.end())
                    candidate_counts[predecessor] = vector<uint64_t>();
                candidate_counts[predecessor].push_back(index);
            }
        }
        vector<Candidate> sorted_candidates;
        for (auto it : candidate_counts)
        {
            sorted_candidates.push_back(Candidate(it.first, ((1.0 * it.second.size()) / (dynamic_cfg->get_bbl_execution_count(it.first)))));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        reverse(sorted_candidates.begin(), sorted_candidates.end());
        if (sorted_candidates.size() < 1)
            return false;
        *result = sorted_candidates[0].predecessor_bbl_address;
        result_miss_index.clear();
        for (auto it : candidate_counts[*result])
        {
            result_miss_index.push_back(it);
        }
        sort(result_miss_index.begin(), result_miss_index.end());
        for (auto it : candidate_counts)
        {
            it.second.clear();
        }
        candidate_counts.clear();
        if (fan_out_ratio != nullptr)
            *fan_out_ratio = sorted_candidates[0].covered_miss_ratio_to_predecessor_count;
        return true;
    }
    bool get_next_candidate(uint64_t *result, Settings *settings, set<uint64_t> &omit_list, CFG *dynamic_cfg, double *fan_out_ratio = nullptr)
    {
        unordered_map<uint64_t, uint64_t> candidate_counts;
        set<uint64_t> candidates;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            all_misses[i]->get_candidates(candidates, settings, dynamic_cfg);
            for (auto it : candidates)
            {
                if (omit_list.find(it) != omit_list.end())
                    continue;
                if (candidate_counts.find(it) == candidate_counts.end())
                    candidate_counts[it] = 1;
                else
                    candidate_counts[it] += 1;
            }
        }
        vector<Candidate> sorted_candidates;
        for (auto it : candidate_counts)
        {
            sorted_candidates.push_back(Candidate(it.first, ((1.0 * it.second) / (dynamic_cfg->get_bbl_execution_count(it.first)))));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        reverse(sorted_candidates.begin(), sorted_candidates.end());
        if (sorted_candidates.size() < 1)
            return false;
        *result = sorted_candidates[0].predecessor_bbl_address;
        if (fan_out_ratio != nullptr)
            *fan_out_ratio = sorted_candidates[0].covered_miss_ratio_to_predecessor_count;
        return true;
    }
    bool get_top_candidate(vector<uint64_t> &result, vector<uint64_t> &result_counts, Settings *settings, CFG *dynamic_cfg)
    {
        result.clear();
        result_counts.clear();
        unordered_map<uint64_t, uint64_t> candidate_counts;
        set<uint64_t> candidates;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            all_misses[i]->get_candidates(candidates, settings, dynamic_cfg);
            for (auto it : candidates)
            {
                if (candidate_counts.find(it) == candidate_counts.end())
                    candidate_counts[it] = 1;
                else
                    candidate_counts[it] += 1;
            }
        }
        vector<Candidate> sorted_candidates;
        for (auto it : candidate_counts)
        {
            sorted_candidates.push_back(Candidate(it.first, it.second));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        reverse(sorted_candidates.begin(), sorted_candidates.end());
        if (sorted_candidates.size() < 1)
            return false;
        for (int i = 0; i < sorted_candidates.size(); i++)
        {
            result.push_back(sorted_candidates[i].predecessor_bbl_address);
            result_counts.push_back(sorted_candidates[i].covered_miss_ratio_to_predecessor_count);
        }
        sorted_candidates.clear();
        candidate_counts.clear();
        return true;
    }
    void get_candidate_counts(unordered_map<uint64_t, uint64_t> &candidate_counts, Settings *settings, set<uint64_t> &omit_list, CFG *dynamic_cfg)
    {
        set<uint64_t> candidates;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            all_misses[i]->get_candidates(candidates, settings, dynamic_cfg);
            for (auto it : candidates)
            {
                if (omit_list.find(it) != omit_list.end())
                    continue;
                if (candidate_counts.find(it) == candidate_counts.end())
                    candidate_counts[it] = 1;
                else
                    candidate_counts[it] += 1;
            }
        }
    }
    uint64_t remove_covered_misses(uint64_t next, Settings *settings, CFG *dynamic_cfg)
    {
        vector<LBR_Sample *> tmp;
        uint64_t total = 0;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            if (all_misses[i]->is_covered(next, settings, dynamic_cfg))
            {
                total += 1;
                all_misses[i]->clear();
            }
            else
            {
                tmp.push_back(all_misses[i]);
            }
        }
        all_misses.clear();
        for (uint64_t i = 0; i < tmp.size(); i++)
        {
            all_misses.push_back(tmp[i]);
        }
        return total;
    }
    uint64_t remove_predicated_covered_misses(uint64_t next, uint64_t predicate, Settings *settings, CFG *dynamic_cfg)
    {
        vector<LBR_Sample *> tmp;
        uint64_t total = 0;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            if (all_misses[i]->is_covered_with_predicate(next, predicate, settings, dynamic_cfg))
            {
                total += 1;
                all_misses[i]->clear();
            }
            else
            {
                tmp.push_back(all_misses[i]);
            }
        }
        all_misses.clear();
        for (uint64_t i = 0; i < tmp.size(); i++)
        {
            all_misses.push_back(tmp[i]);
        }
        return total;
    }
    bool get_top_correlated_candidate(uint64_t *result, uint64_t *covered_miss_count, uint64_t next, uint64_t next_count, Settings *settings, CFG *dynamic_cfg)
    {
        unordered_map<uint64_t, uint64_t> candidate_counts;
        set<uint64_t> candidates;
        for (uint64_t i = 0; i < all_misses.size(); i++)
        {
            all_misses[i]->get_correlated_candidates(next, candidates, settings, dynamic_cfg);
            for (auto it : candidates)
            {
                if (it == next)
                    continue;
                if (candidate_counts.find(it) == candidate_counts.end())
                    candidate_counts[it] = 1;
                else
                    candidate_counts[it] += 1;
            }
        }
        vector<Candidate> sorted_candidates;
        for (auto it : candidate_counts)
        {
            sorted_candidates.push_back(Candidate(it.first, it.second));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        reverse(sorted_candidates.begin(), sorted_candidates.end());
        if (sorted_candidates.size() < 1)
            return false;
        //if(sorted_candidates[0].covered_miss_ratio_to_predecessor_count < (next_count * max(0.8,settings->fan_out_ratio)))return false;
        *result = sorted_candidates[0].predecessor_bbl_address;
        *covered_miss_count = sorted_candidates[0].covered_miss_ratio_to_predecessor_count;
        sorted_candidates.clear();
        candidate_counts.clear();
        return true;
    }
};

class Miss_Map
{
private:
    ofstream *bbl_mapping_out = nullptr;
    ofstream *context_sensitive_bbl_out = nullptr;
    ofstream *fan_out_index_out = nullptr;

public:
    unordered_map<pair<uint64_t, uint64_t>, LBR_List *, boost::hash<pair<uint64_t, uint64_t>>> missed_pc_to_LBR_sample_list;
    vector<pair<uint64_t, uint64_t>> miss_profile;
    Miss_Map()
    {
        missed_pc_to_LBR_sample_list.clear();
    }
    ~Miss_Map()
    {
        if (bbl_mapping_out != nullptr)
            bbl_mapping_out->close();
        missed_pc_to_LBR_sample_list.clear();
        miss_profile.clear();
    }
    void set_bbl_mapping_out(string file_name)
    {
        if (bbl_mapping_out == nullptr)
        {
            bbl_mapping_out = new ofstream(file_name.c_str());
        }
    }
    void set_context_sensitive_bbl_out(string file_name)
    {
        if (context_sensitive_bbl_out == nullptr)
        {
            context_sensitive_bbl_out = new ofstream(file_name.c_str());
        }
    }
    void set_fan_out_index_out(string file_name)
    {
        if (fan_out_index_out == nullptr)
        {
            fan_out_index_out = new ofstream(file_name.c_str());
        }
    }
    uint64_t size()
    {
        return missed_pc_to_LBR_sample_list.size();
    }
    uint64_t total_misses()
    {
        uint64_t total = 0;
        for (auto it : missed_pc_to_LBR_sample_list)
        {
            total += it.second->size();
        }
        return total;
    }
    void push(string line, CFG &dynamic_cfg)
    {
        boost::trim_if(line, boost::is_any_of(","));
        vector<string> parsed;
        boost::split(parsed, line, boost::is_any_of(",\n"), boost::token_compress_on);
        if (parsed.size() < 3)
            panic("ICache Miss LBR Sample contains less than 3 branch records");
        uint64_t missed_cache_line_id = string_to_u64(parsed[0]);
        uint64_t missed_bbl_address = string_to_u64(parsed[1]);
        //if(dynamic_cfg.is_self_modifying(missed_bbl_address))return;
        miss_profile.push_back(make_pair(missed_cache_line_id, missed_bbl_address));
        if ((missed_bbl_address >> 6) != missed_cache_line_id)
        {
            //panic("In access based next line prefetching system a icache miss cannot happen other than the start of a new BBL");
            //missed_bbl_address = missed_cache_line_id<<6;
        }
        if (missed_pc_to_LBR_sample_list.find(make_pair(missed_cache_line_id, missed_bbl_address)) == missed_pc_to_LBR_sample_list.end())
        {
            missed_pc_to_LBR_sample_list[make_pair(missed_cache_line_id, missed_bbl_address)] = new LBR_List();
        }
        missed_pc_to_LBR_sample_list[make_pair(missed_cache_line_id, missed_bbl_address)]->push(parsed);
    }
    void measure_prefetch_window_cdf(Settings *settings, CFG *dynamic_cfg, ofstream &out)
    {
        unordered_map<uint64_t, uint64_t> table;
        for (auto it : missed_pc_to_LBR_sample_list)
        {
            it.second->measure_prefetch_window_cdf(table, settings, dynamic_cfg);
        }
        uint64_t total = 0;
        vector<uint64_t> sorted_counts;
        for (auto it : table)
        {
            total += it.second;
            sorted_counts.push_back(it.first);
        }
        sort(sorted_counts.begin(), sorted_counts.end());
        double value = 0.0;
        for (uint64_t i = 0; i < sorted_counts.size(); i++)
        {
            value += table[sorted_counts[i]];
            out << sorted_counts[i] << " " << ((100.0 * value) / total) << endl;
        }
    }
    void generate_prefetch_locations(Settings *settings, CFG *dynamic_cfg, ofstream &out)
    {
        unordered_map<uint64_t, vector<uint64_t>> bbl_to_prefetch_targets;

        vector<pair<uint64_t, pair<uint64_t, uint64_t>>> sorted_miss_pcs;
        for (auto it : missed_pc_to_LBR_sample_list)
        {
            sorted_miss_pcs.push_back(make_pair(it.second->size(), it.first));
        }

        sort(sorted_miss_pcs.begin(), sorted_miss_pcs.end());
        reverse(sorted_miss_pcs.begin(), sorted_miss_pcs.end());

        uint64_t total_miss_sample_count = total_misses();

        bool enable_code_bloat = false;

        if (settings->multiline_mode == 2) // Our
        {
            if (settings->our_mode == ALL)
            {
                bool prefetch_count_finished = false;
                bbl_to_prefetch_targets.clear();
                uint64_t total_covered_miss_count = 0;

                Classifier classifier;

                for (int i = 0; i < sorted_miss_pcs.size(); i++)
                {
                    if (prefetch_count_finished)
                        break;
                    auto missed_pc = sorted_miss_pcs[i].second;
                    uint64_t missed_count_for_this_pc = sorted_miss_pcs[i].first;
                    uint64_t prefetch_candidate;
                    double fan_out_ratio;
                    set<uint64_t> omit_list;
                    if (settings->logging)
                        cerr << missed_pc.first << "," << missed_pc.second << "," << missed_count_for_this_pc << endl;
                    while (missed_count_for_this_pc > 0)
                    {
                        vector<uint64_t> good_indexes;
                        bool candidate_found = missed_pc_to_LBR_sample_list[missed_pc]->get_next_candidate(&prefetch_candidate, good_indexes, settings, omit_list, dynamic_cfg, &fan_out_ratio);
                        if (!candidate_found)
                            break;

                        omit_list.insert(prefetch_candidate);

                        if (fan_out_ratio < settings->fan_out_ratio)
                            continue;
                        if (dynamic_cfg->is_self_modifying(prefetch_candidate))
                            continue;

                        uint64_t prefetched_cache_line_address = (missed_pc.first) << 6;

                        if (fan_out_ratio < 1.0)
                        {
                            classifier.add_new_filtered_bbls(prefetch_candidate, fan_out_ratio, good_indexes, prefetched_cache_line_address);
                        }
                        if (bbl_to_prefetch_targets.find(prefetch_candidate) == bbl_to_prefetch_targets.end())
                            bbl_to_prefetch_targets[prefetch_candidate] = vector<uint64_t>();
                        else if (bbl_to_prefetch_targets[prefetch_candidate].size() > 0)
                        {
                            uint64_t first_inserted_target = bbl_to_prefetch_targets[prefetch_candidate][0];
                            int prefetch_length = measure_prefetch_length(prefetched_cache_line_address, first_inserted_target);
                        }
                        bbl_to_prefetch_targets[prefetch_candidate].push_back(prefetched_cache_line_address);

                        uint64_t covered_miss_count = missed_pc_to_LBR_sample_list[missed_pc]->remove_covered_misses(prefetch_candidate, settings, dynamic_cfg);
                        if (settings->logging)
                            cerr << prefetch_candidate << "," << covered_miss_count << "," << dynamic_cfg->get_fan_out(prefetch_candidate, missed_pc.second) << endl;
                        missed_count_for_this_pc -= covered_miss_count;
                        total_covered_miss_count += covered_miss_count;
                    }
                    if (settings->logging)
                        cerr << "0" << endl;
                    if (settings->logging)
                        cerr << missed_count_for_this_pc << endl;
                }

                cerr << total_miss_sample_count << "," << total_covered_miss_count << "," << total_misses() << endl;

                unordered_map<uint64_t, uint64_t> prev_to_new;
                dynamic_cfg->print_modified_bbl_mappings(bbl_to_prefetch_targets, 7, bbl_mapping_out, prev_to_new, enable_code_bloat);

                for (auto it : bbl_to_prefetch_targets)
                {
                    if (it.second.size() < 1)
                        continue;
                    out << prev_to_new[it.first] << " " << it.second.size();
                    sort(bbl_to_prefetch_targets[it.first].begin(), bbl_to_prefetch_targets[it.first].end());
                    for (auto set_it : it.second)
                    {
                        if (enable_code_bloat)
                            out << " " << prev_to_new[set_it];
                        else
                            out << " " << set_it;
                    }
                    out << endl;
                }
                classifier.add_full_ordered_bbls(dynamic_cfg->get_full_ordered_bbls(), settings->context_size);

                cerr << total_miss_sample_count << "," << total_covered_miss_count << "," << total_misses() << endl;

                cerr << total_covered_miss_count << "," << (100.0 * total_covered_miss_count) / total_miss_sample_count << endl;
            }
            else if (settings->our_mode == ONLY_COALESCE)
            {
                bool prefetch_count_finished = false;
                bbl_to_prefetch_targets.clear();
                uint64_t total_covered_miss_count = 0;
                for (int i = 0; i < sorted_miss_pcs.size(); i++)
                {
                    if (prefetch_count_finished)
                        break;
                    auto missed_pc = sorted_miss_pcs[i].second;
                    uint64_t missed_count_for_this_pc = sorted_miss_pcs[i].first;
                    uint64_t prefetch_candidate;
                    double fan_out_ratio;
                    set<uint64_t> omit_list;
                    while (missed_count_for_this_pc > 0)
                    {
                        bool candidate_found = missed_pc_to_LBR_sample_list[missed_pc]->get_next_candidate(&prefetch_candidate, settings, omit_list, dynamic_cfg, &fan_out_ratio);
                        if (!candidate_found)
                            break;

                        omit_list.insert(prefetch_candidate);

                        if (fan_out_ratio < settings->fan_out_ratio)
                            continue;
                        if (dynamic_cfg->is_self_modifying(prefetch_candidate))
                            continue;

                        uint64_t prefetched_cache_line_address = (missed_pc.first) << 6;

                        if (bbl_to_prefetch_targets.find(prefetch_candidate) == bbl_to_prefetch_targets.end())
                            bbl_to_prefetch_targets[prefetch_candidate] = vector<uint64_t>();
                        else if (bbl_to_prefetch_targets[prefetch_candidate].size() > 0)
                        {
                            uint64_t first_inserted_target = bbl_to_prefetch_targets[prefetch_candidate][0];
                            int prefetch_length = measure_prefetch_length(prefetched_cache_line_address, first_inserted_target);
                            if (my_abs(prefetch_length) <= settings->max_prefetch_length)
                            {
                                //
                            }
                            else
                                continue;
                        }

                        bbl_to_prefetch_targets[prefetch_candidate].push_back((missed_pc.first) << 6);
                        uint64_t covered_miss_count = missed_pc_to_LBR_sample_list[missed_pc]->remove_covered_misses(prefetch_candidate, settings, dynamic_cfg);
                        missed_count_for_this_pc -= covered_miss_count;
                        total_covered_miss_count += covered_miss_count;
                    }
                }

                unordered_map<uint64_t, uint64_t> prev_to_new;
                dynamic_cfg->print_modified_bbl_mappings(bbl_to_prefetch_targets, 7, bbl_mapping_out, prev_to_new, enable_code_bloat);
                for (auto it : bbl_to_prefetch_targets)
                {
                    if (it.second.size() < 1)
                        continue;
                    out << prev_to_new[it.first] << " " << it.second.size();
                    for (auto set_it : it.second)
                    {
                        if (enable_code_bloat)
                            out << " " << prev_to_new[set_it];
                        else
                            out << " " << set_it;
                    }
                    out << endl;
                }
                cerr << total_covered_miss_count << "," << (100.0 * total_covered_miss_count) / total_miss_sample_count << endl;
            }
            else if (settings->our_mode == ONLY_CONTEXT)
            {
                bool prefetch_count_finished = false;
                bbl_to_prefetch_targets.clear();
                uint64_t total_covered_miss_count = 0;

                Classifier classifier;

                for (int i = 0; i < sorted_miss_pcs.size(); i++)
                {
                    if (prefetch_count_finished)
                        break;
                    auto missed_pc = sorted_miss_pcs[i].second;
                    uint64_t missed_count_for_this_pc = sorted_miss_pcs[i].first;
                    uint64_t prefetch_candidate;
                    double fan_out_ratio;
                    set<uint64_t> omit_list;
                    if (settings->logging)
                        cerr << missed_pc.first << "," << missed_pc.second << "," << missed_count_for_this_pc << endl;
                    while (missed_count_for_this_pc > 0)
                    {
                        vector<uint64_t> good_indexes;
                        bool candidate_found = missed_pc_to_LBR_sample_list[missed_pc]->get_next_candidate(&prefetch_candidate, good_indexes, settings, omit_list, dynamic_cfg, &fan_out_ratio);
                        if (!candidate_found)
                            break;

                        omit_list.insert(prefetch_candidate);

                        if (fan_out_ratio < settings->fan_out_ratio)
                            continue;
                        if (dynamic_cfg->is_self_modifying(prefetch_candidate))
                            continue;

                        uint64_t prefetched_cache_line_address = (missed_pc.first) << 6;

                        if (fan_out_ratio < 1.0)
                        {
                            classifier.add_new_filtered_bbls(prefetch_candidate, fan_out_ratio, good_indexes, prefetched_cache_line_address);
                        }
                        if (bbl_to_prefetch_targets.find(prefetch_candidate) == bbl_to_prefetch_targets.end())
                            bbl_to_prefetch_targets[prefetch_candidate] = vector<uint64_t>();
                        else
                            continue;
                        bbl_to_prefetch_targets[prefetch_candidate].push_back(prefetched_cache_line_address);

                        uint64_t covered_miss_count = missed_pc_to_LBR_sample_list[missed_pc]->remove_covered_misses(prefetch_candidate, settings, dynamic_cfg);
                        if (settings->logging)
                            cerr << prefetch_candidate << "," << covered_miss_count << "," << dynamic_cfg->get_fan_out(prefetch_candidate, missed_pc.second) << endl;
                        missed_count_for_this_pc -= covered_miss_count;
                        total_covered_miss_count += covered_miss_count;
                    }
                    if (settings->logging)
                        cerr << "0" << endl;
                    if (settings->logging)
                        cerr << missed_count_for_this_pc << endl;
                }

                cerr << total_miss_sample_count << "," << total_covered_miss_count << "," << total_misses() << endl;

                unordered_map<uint64_t, uint64_t> prev_to_new;
                dynamic_cfg->print_modified_bbl_mappings(bbl_to_prefetch_targets, 7, bbl_mapping_out, prev_to_new, enable_code_bloat);

                for (auto it : bbl_to_prefetch_targets)
                {
                    if (it.second.size() < 1)
                        continue;
                    out << prev_to_new[it.first] << " " << it.second.size();
                    sort(bbl_to_prefetch_targets[it.first].begin(), bbl_to_prefetch_targets[it.first].end());
                    for (auto set_it : it.second)
                    {
                        if (enable_code_bloat)
                            out << " " << prev_to_new[set_it];
                        else
                            out << " " << set_it;
                    }
                    out << endl;
                }
                classifier.add_full_ordered_bbls(dynamic_cfg->get_full_ordered_bbls(), settings->context_size);

                cerr << total_miss_sample_count << "," << total_covered_miss_count << "," << total_misses() << endl;

                cerr << total_covered_miss_count << "," << (100.0 * total_covered_miss_count) / total_miss_sample_count << endl;
            }
        }
        else if (settings->multiline_mode == 1) //AsmDB
        {
            bool prefetch_count_finished = false;
            bbl_to_prefetch_targets.clear();
            uint64_t total_covered_miss_count = 0;
            for (int i = 0; i < sorted_miss_pcs.size(); i++)
            {
                if (prefetch_count_finished)
                    break;
                auto missed_pc = sorted_miss_pcs[i].second;
                uint64_t missed_count_for_this_pc = sorted_miss_pcs[i].first;
                uint64_t prefetch_candidate;
                double fan_out_ratio;
                set<uint64_t> omit_list;
                while (missed_count_for_this_pc > 0)
                {
                    bool candidate_found = missed_pc_to_LBR_sample_list[missed_pc]->get_next_candidate(&prefetch_candidate, settings, omit_list, dynamic_cfg, &fan_out_ratio);
                    if (!candidate_found)
                        break;

                    omit_list.insert(prefetch_candidate);

                    if (fan_out_ratio < settings->fan_out_ratio)
                        continue;
                    if (dynamic_cfg->is_self_modifying(prefetch_candidate))
                        continue;

                    if (bbl_to_prefetch_targets.find(prefetch_candidate) == bbl_to_prefetch_targets.end())
                        bbl_to_prefetch_targets[prefetch_candidate] = vector<uint64_t>();
                    else
                        continue;

                    bbl_to_prefetch_targets[prefetch_candidate].push_back((missed_pc.first) << 6);
                    uint64_t covered_miss_count = missed_pc_to_LBR_sample_list[missed_pc]->remove_covered_misses(prefetch_candidate, settings, dynamic_cfg);
                    missed_count_for_this_pc -= covered_miss_count;
                    total_covered_miss_count += covered_miss_count;
                }
            }

            unordered_map<uint64_t, uint64_t> prev_to_new;
            dynamic_cfg->print_modified_bbl_mappings(bbl_to_prefetch_targets, 7, bbl_mapping_out, prev_to_new, enable_code_bloat);
            for (auto it : bbl_to_prefetch_targets)
            {
                if (it.second.size() < 1)
                    continue;
                out << prev_to_new[it.first] << " " << it.second.size();
                for (auto set_it : it.second)
                {
                    if (enable_code_bloat)
                        out << " " << prev_to_new[set_it];
                    else
                        out << " " << set_it;
                }
                out << endl;
            }
            cerr << total_covered_miss_count << "," << (100.0 * total_covered_miss_count) / total_miss_sample_count << endl;
        }
    }
};

class Miss_Profile
{
public:
    Miss_Map profile;
    Miss_Profile(MyConfigWrapper &conf, Settings &settings, CFG &dynamic_cfg, bool measure_prefetch_window_cdf = false)
    {
        string miss_log_path = conf.lookup("miss_log", "/tmp/");
        if (miss_log_path == "/tmp/")
        {
            panic("ICache Miss Log file is not present in the config");
        }
        vector<string> raw_icache_miss_lbr_sample_data;
        read_full_file(miss_log_path, raw_icache_miss_lbr_sample_data);
        for (uint64_t i = 0; i < raw_icache_miss_lbr_sample_data.size(); i++)
        {
            string line = raw_icache_miss_lbr_sample_data[i];
            profile.push(line, dynamic_cfg);
        }
        string output_name = "";
        output_name += to_string(settings.multiline_mode) + ".";
        output_name += to_string(settings.min_distance_cycle_count) + ".";
        output_name += to_string(settings.max_distance_cycle_count) + ".";
        output_name += to_string(int(settings.fan_out_ratio * 1000)) + ".";
        output_name += "txt";
        ofstream prefetch_out(output_name.c_str());
        profile.set_bbl_mapping_out(("bbl." + output_name));
        profile.set_context_sensitive_bbl_out(("cs." + output_name));
        if (measure_prefetch_window_cdf)
        {
            ofstream prefetch_window_accessed_cache_line_count_cdf("prefetch_window_accessed_cache_line_count_cdf.txt");
            profile.measure_prefetch_window_cdf(&settings, &dynamic_cfg, prefetch_window_accessed_cache_line_count_cdf);
        }
        profile.generate_prefetch_locations(&settings, &dynamic_cfg, prefetch_out);
    }
};

#endif //LBR_MISS_SAMPLE_H_
