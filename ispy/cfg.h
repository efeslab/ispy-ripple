#ifndef CFG_H_
#define CFG_H_
#include <bits/stdc++.h>

using namespace std;

#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>

#include "config.h"
#include "log.h"
#include "gz_reader.h"
#include "convert.h"
#include "settings.h"

#include "aho_corasick.hpp"

namespace ac = aho_corasick;
using utrie = ac::utrie;

class BBL
{
public:
    uint64_t total_count;
    unordered_map<uint64_t,uint64_t> neighbor_count;
    uint32_t instrs;
    uint32_t bytes;
    uint64_t average_cycles;
    BBL(uint32_t instr_count, uint32_t byte_size)
    {
        total_count=0;
        instrs=instr_count;
        bytes=byte_size;
        average_cycles=instr_count;
    }
    void increment()
    {
        total_count+=1;
    }
    void add_neighbor(uint64_t neighbor_bbl)
    {
        if(neighbor_count.find(neighbor_bbl)==neighbor_count.end())
        {
            neighbor_count[neighbor_bbl]=1;
        }
        else
        {
            neighbor_count[neighbor_bbl]+=1;
        }
    }
    double get_neighbor_ratio(uint64_t neighbor_bbl)
    {
        if(total_count<1)panic("Neighbor ratio is called on a BBL with 0 dynamic count");
        double denominator = total_count;
        double numerator = 0;
        if(neighbor_count.find(neighbor_bbl)!=neighbor_count.end())numerator+=neighbor_count[neighbor_bbl];
        return numerator/denominator;
    }
    uint64_t get_count()
    {
        return total_count;
    }
};

class CFG
{
private:
    unordered_map<uint64_t,BBL *> bbl_infos;
    uint64_t static_size;
    uint64_t static_count;
    Settings *sim_settings;
    set<uint64_t> self_modifying_bbls;
    vector<uint64_t> full_ordered_bbls;
    unordered_map<uint64_t,vector<uint64_t>> bbl_locations_in_the_ordered_log;
public:
    vector<uint64_t> & get_full_ordered_bbls()
    {
        return full_ordered_bbls;
    }
    CFG(MyConfigWrapper &conf, Settings &settings)
    {
        sim_settings = &settings;

        string self_modifying_bbl_info_path = conf.lookup("bbl_info_self_modifying", "/tmp/");
        if(self_modifying_bbl_info_path == "/tmp/")
        {
            //do nothing
            //since self modifying bbls are optional only for jitted codes
        }
        else
        {
            vector<string> raw_self_modifying_bbl_info_data;
            read_full_file(self_modifying_bbl_info_path, raw_self_modifying_bbl_info_data);
            for(uint64_t i = 0; i<raw_self_modifying_bbl_info_data.size(); i++)
            {
                string line = raw_self_modifying_bbl_info_data[i];
                boost::trim_if(line,boost::is_any_of(","));
                vector<string> parsed;
                boost::split(parsed,line,boost::is_any_of(",\n"),boost::token_compress_on);
                if(parsed.size()!=3)
                {
                    panic("A line on the self-modifying BBL info file does not have exactly three tuples");
                }
                uint64_t bbl_address = string_to_u64(parsed[0]);
                self_modifying_bbls.insert(bbl_address);
            }
            raw_self_modifying_bbl_info_data.clear();
        }

        string bbl_info_path = conf.lookup("bbl_info", "/tmp/");
        if(bbl_info_path == "/tmp/")
        {
            panic("BBL Info file is not present in the config");
        }
        vector<string> raw_bbl_info_data;
        read_full_file(bbl_info_path, raw_bbl_info_data);
        static_count = 0;
        static_size = 0;
        for(uint64_t i = 0; i<raw_bbl_info_data.size(); i++)
        {
            string line = raw_bbl_info_data[i];
            boost::trim_if(line,boost::is_any_of(","));
            vector<string> parsed;
            boost::split(parsed,line,boost::is_any_of(",\n"),boost::token_compress_on);
            if(parsed.size()!=3)
            {
                panic("A line on the BBL info file does not have exactly three tuples");
            }
            uint64_t bbl_address = string_to_u64(parsed[0]);
            uint32_t instrs = string_to_u32(parsed[1]);
            static_count+=instrs;
            uint32_t bytes = string_to_u32(parsed[2]);
            static_size+=bytes;
            if(bbl_infos.find(bbl_address)==bbl_infos.end())
            {
                bbl_infos[bbl_address]=new BBL(instrs, bytes);
            }
            else
            {
                panic("BBL info file includes multiple info entries for the same BBL start address");
            }
            parsed.clear();
        }
        raw_bbl_info_data.clear();

        full_ordered_bbls.clear();
        string bbl_log_path = conf.lookup("bbl_log", "/tmp/");
        if(bbl_log_path == "/tmp/")
        {
            panic("BBL Log file is not present in the config");
        }
        vector<string> raw_bbl_log_data;
        read_full_file(bbl_log_path, raw_bbl_log_data);
        uint64_t prev[] = {0, 0};
        unordered_map<uint64_t,pair<uint64_t,uint64_t>> sum_counts;
        for(uint64_t i = 0; i< raw_bbl_log_data.size(); i++)
        {
            string line = raw_bbl_log_data[i];
            boost::trim_if(line,boost::is_any_of(","));
            vector<string> parsed;
            boost::split(parsed,line,boost::is_any_of(",\n"),boost::token_compress_on);
            if(parsed.size()!=2)
            {
                panic("A line on the BBL log file does not have exactly two tuples");
            }
            uint64_t bbl_address = string_to_u64(parsed[0]);
            uint64_t cycle = string_to_u64(parsed[1]);
            if(prev[0]!=0)
            {
                if(sum_counts.find(prev[0])!=sum_counts.end())
                {
                    sum_counts[prev[0]].first+=cycle;
                    sum_counts[prev[0]].second++;
                }
                else
                {
                    sum_counts[prev[0]]=make_pair(cycle,1);
                }
            }
            prev[0]=prev[1];
            prev[1]=bbl_address;
            full_ordered_bbls.push_back(bbl_address);
            parsed.clear();
        }
        raw_bbl_log_data.clear();

        for(auto it: sum_counts)
        {
            uint64_t bbl_address = it.first;
            uint64_t sum = it.second.first;
            uint64_t count = it.second.second;
            if(bbl_infos.find(bbl_address)==bbl_infos.end())
            {
                panic("BBL log file includes a BBL that is not present in BBL info file");
            }
            else
            {
                bbl_infos[bbl_address]->average_cycles = 1 + ((sum - 1) / count);
            }
        }
        sum_counts.clear();

        uint64_t min_distance = sim_settings->get_min_distance();
        uint64_t max_distance = sim_settings->get_max_distance();

        for(uint64_t i = 0; i < full_ordered_bbls.size(); i++)
        {
            uint64_t bbl_address = full_ordered_bbls[i];
            if(bbl_infos.find(bbl_address)==bbl_infos.end())
            {
                panic("BBL log file includes a BBL that is not present in BBL info file");
            }
            else
            {
                bbl_infos[bbl_address]->increment();
            }
            if(bbl_locations_in_the_ordered_log.find(bbl_address)==bbl_locations_in_the_ordered_log.end())
            {
                bbl_locations_in_the_ordered_log[bbl_address]=vector<uint64_t>();
            }
            bbl_locations_in_the_ordered_log[bbl_address].push_back(i);
            uint64_t current_distance = 0;
            set<uint64_t> already_added_neighbors;
            for(uint64_t j = i+1; j < full_ordered_bbls.size(); j++)
            {
                if(sim_settings->multiline_mode == 1||sim_settings->remove_lbr_cycle==true)//ASMDB
                {
                    current_distance+=bbl_infos[full_ordered_bbls[j-1]]->instrs;
                }
                else // if(settings.multiline_mode == 2) OUR
                {
                    current_distance+=bbl_infos[full_ordered_bbls[j-1]]->average_cycles;
                }
                if(current_distance<min_distance)continue;
                if(already_added_neighbors.find(full_ordered_bbls[j])!=already_added_neighbors.end())continue;
                bbl_infos[bbl_address]->add_neighbor(full_ordered_bbls[j]);
                already_added_neighbors.insert(full_ordered_bbls[j]);
                if(current_distance>max_distance)break;
            }
            already_added_neighbors.clear();
        }
    }
    bool is_self_modifying(uint64_t bbl_address)
    {
        return self_modifying_bbls.find(bbl_address) != self_modifying_bbls.end();
    }
    double get_fan_out(uint64_t predecessor_bbl_address, uint64_t successor_bbl_address)
    {
        if(bbl_infos.find(predecessor_bbl_address)==bbl_infos.end())
        {
            panic("Trying to calculate fan-out of a predecessor basic block not present in the trace");
        }
        return bbl_infos[predecessor_bbl_address]->get_neighbor_ratio(successor_bbl_address);
    }
    bool is_valid_candidate(uint64_t predecessor_bbl_address, uint64_t successor_bbl_address)
    {
        if(is_self_modifying(predecessor_bbl_address))return false;
        return get_fan_out(predecessor_bbl_address, successor_bbl_address) >= sim_settings->fan_out_ratio;
    }
    uint64_t get_bbl_execution_count(uint64_t bbl_address)
    {
        if(bbl_infos.find(bbl_address)==bbl_infos.end())
        {
            panic("Trying to get execution count for a basic block not present in the trace");
        }
        return bbl_infos[bbl_address]->get_count();
    }
    uint64_t get_static_count()
    {
        return static_count;
    }
    uint64_t get_static_size()
    {
        return static_size;
    }
    uint32_t get_bbl_instr_count(uint64_t bbl_address)
    {
        if(bbl_infos.find(bbl_address)==bbl_infos.end())
        {
            panic("Trying to get instruction count for a basic block not present in the bbl-info or trace");
        }
        return bbl_infos[bbl_address]->instrs;
    }
    uint64_t get_bbl_avg_cycles(uint64_t bbl_address)
    {
        return bbl_infos[bbl_address]->average_cycles;
    }
    uint64_t get_bbl_size(uint64_t bbl_address)
    {
        return bbl_infos[bbl_address]->bytes;
    }
    void get_context_execution_count(vector<vector<vector<uint64_t>>> &all_contexts, vector<vector<uint64_t>> &all_contexts_execution_counts)
    {
        utrie t;
        basic_string<uint64_t> tmp;

        vector<uint64_t> t_match_counts;

        struct container_hash {
            std::size_t operator()(const vector<uint64_t>& c) const {
                return boost::hash_range(c.begin(), c.end());
            }
        };

        unordered_map<vector<uint64_t>,uint64_t,container_hash> context_to_index;

        for(uint64_t i =0; i<all_contexts.size(); i++)
        {
            for(uint64_t j=0; j<all_contexts[i].size(); j++)
            {
                context_to_index[all_contexts[i][j]]=0;
                all_contexts_execution_counts[i].push_back(0);
            }
        }

        tmp.clear();

        uint64_t total_insert_in_t = 0;

        for(auto key_val: context_to_index)
        {
            auto &key = key_val.first;
            for(auto it: key)tmp.push_back(it);
            t.insert(tmp);
            tmp.clear();
            context_to_index[key] = total_insert_in_t;
            t_match_counts.push_back(0);
            total_insert_in_t++;
        }
        tmp.clear();
        for(auto it: full_ordered_bbls)
        {
            tmp.push_back(it);
        }
        auto matches = t.parse_text(tmp);
        tmp.clear();
        for(auto match: matches)
        {
            t_match_counts[match.get_index()]++;
        }
        matches.clear();

        for(uint64_t i =0; i<all_contexts.size(); i++)
        {
            for(uint64_t j=0; j<all_contexts[i].size(); j++)
            {
                all_contexts_execution_counts[i][j]=t_match_counts[context_to_index[all_contexts[i][j]]];
            }
        }
        tmp.clear();
        t_match_counts.clear();
        context_to_index.clear();
    }
    uint64_t get_multiple_bbl_execution_count(uint64_t candidate, uint64_t prior, bool &valid)
    {
        if(is_self_modifying(candidate)||is_self_modifying(prior)||bbl_locations_in_the_ordered_log.find(candidate)==bbl_locations_in_the_ordered_log.end())
        {
            valid = false;
            return 0;
        }
        uint64_t down = 0;
        for(uint64_t i = 0; i<bbl_locations_in_the_ordered_log[candidate].size(); i++)
        {
            uint64_t index = bbl_locations_in_the_ordered_log[candidate][i];
            for(int j = index - 1; ((j > index - 9) && (j > -1)); j--)
            {
                //last_eight_bbls.insert(full_ordered_bbls[j]);
                if(full_ordered_bbls[j]==prior)
                {
                    down+=1;
                    break;
                }
            }
        }
        valid=true;
        return down;
    }
    double get_fan_in_for_multiple_prior_bbls(uint64_t miss_target, uint64_t candidate, uint64_t prior, uint64_t *dynamic_prefetch_counts)
    {
        if(is_self_modifying(candidate))return 0;
        if(is_self_modifying(prior))return 0;
        if(bbl_locations_in_the_ordered_log.find(candidate)==bbl_locations_in_the_ordered_log.end())return 0;
        uint64_t down = 0;
        uint64_t up = 0;

        uint64_t min_distance = sim_settings->get_min_distance();
        uint64_t max_distance = sim_settings->get_max_distance();
        for(uint64_t i = 0; i<bbl_locations_in_the_ordered_log[candidate].size(); i++)
        {
            uint64_t index = bbl_locations_in_the_ordered_log[candidate][i];
            bool prior_found = false;
            for(int j = index - 1; ((j > index - 9) && (j > -1)); j--)
            {
                //last_eight_bbls.insert(full_ordered_bbls[j]);
                if(full_ordered_bbls[j]==prior)
                {
                    prior_found = true;
                    break;
                }
            }
            if(prior_found)
            {
                down+=1;

                uint64_t current_distance = 0;
                for(uint64_t j = index+1; j < full_ordered_bbls.size(); j++)
                {
                    if(sim_settings->multiline_mode == 1||sim_settings->remove_lbr_cycle==true)//ASMDB
                    {
                        current_distance+=bbl_infos[full_ordered_bbls[j-1]]->instrs;
                    }
                    else // if(settings.multiline_mode == 2) OUR
                    {
                        current_distance+=bbl_infos[full_ordered_bbls[j-1]]->average_cycles;//ordered_bbl_trace[j].second;
                    }
                    if(current_distance<min_distance)continue;
                    if(full_ordered_bbls[j]==miss_target)
                    {
                        up+=1;
                        break;
                    }
                    if(current_distance>max_distance)break;
                }
            }
        }
        *dynamic_prefetch_counts = down;
        if(down==0 || up==0)return 0;
        double result = 1.0;
        result *=up;
        result /=down;
        return result;
    }
    void print_modified_bbl_mappings(unordered_map<uint64_t,vector<uint64_t>> &bbl_to_prefetch_targets, uint8_t size_of_prefetch_inst, ofstream *out, unordered_map<uint64_t,uint64_t> &prev_to_new, bool enable_code_bloat = true)
    {
        prev_to_new.clear();
        vector<uint64_t> sorted_bbl_addrs;
        for(auto it: bbl_infos)
        {
            sorted_bbl_addrs.push_back(it.first);
        }
        sort(sorted_bbl_addrs.begin(), sorted_bbl_addrs.end());

        uint64_t start_address = 0;
        for(uint32_t i=0; i<sorted_bbl_addrs.size(); i++)
        {
            uint64_t prev_bbl_address = sorted_bbl_addrs[i];
            uint64_t new_bbl_address = prev_bbl_address;
            if(start_address<=new_bbl_address)
            {
                // no offset due to upper bbls
                start_address = new_bbl_address;
            }
            else
            {
                // offset due to upper bbls
                new_bbl_address = start_address;
            }

            if(enable_code_bloat)prev_to_new[prev_bbl_address] = new_bbl_address;
            else prev_to_new[prev_bbl_address] = prev_bbl_address;
            if(out!=nullptr)*out<<prev_bbl_address<<" "<<prev_to_new[prev_bbl_address]<<endl;

            start_address += get_bbl_size(prev_bbl_address);
            if(bbl_to_prefetch_targets.find(prev_bbl_address)!=bbl_to_prefetch_targets.end())
            {
                if(bbl_to_prefetch_targets[prev_bbl_address].size()>0)
                {
                    start_address+=size_of_prefetch_inst;
                }
            }
        }
    }
};
#endif //CFG_H_
