#ifndef CLASSIFIER_H_
#define CLASSIFIER_H_
#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>
#include "gz_reader.h"
#include "convert.h"
#include "hash.h"

using namespace std;

#define LBR_CAPACITY 32

struct MissInfo
{
    double fan_out;
    set<uint64_t> positive_indexes;
    uint64_t target;
};

struct Result
{
    bool has_found;
    uint64_t useful_prefetch;
    uint64_t total_prefetch;
    uint64_t nums;
    set<uint64_t> context;
};

#define FP_SIZE 7
class FP_Result
{
public:
    uint64_t log_table[11];// = {1,2,4,8,16,32,64,128,256,512,1024};
    uint64_t fp[FP_SIZE];
    uint64_t tn[FP_SIZE];
    uint64_t size[FP_SIZE];
    FP_Result()
    {
        log_table[0]=1;
        log_table[1]=2;
        log_table[2]=4;
        log_table[3]=8;
        log_table[4]=16;
        log_table[5]=32;
        log_table[6]=64;
        log_table[7]=128;
        log_table[8]=256;
        log_table[9]=512;
        log_table[10]=1024;
        for(int i=0; i<FP_SIZE; i++)
        {
            fp[i]=0;
            tn[i]=0;
            size[i]=0;
        }
    }
};

class Classifier
{
private:
    vector<uint64_t> full_ordered_bbls;
    unordered_map<uint64_t,unordered_map<uint64_t,set<uint64_t>>> bbl_index;
    unordered_map<uint64_t,vector<MissInfo *>> filtered_bbls;
    void add_into_bbl_index(deque<uint64_t> &current_lbr)
    {
        uint64_t index = full_ordered_bbls.size() - 1;
        uint64_t bbl = full_ordered_bbls[index];
        if(bbl_index.find(bbl)==bbl_index.end())
        {
            bbl_index[bbl]=unordered_map<uint64_t,set<uint64_t>>();
        }
        auto &next = bbl_index[bbl];
        next[index] = set<uint64_t>();
        for(auto it: current_lbr)
        {
            next[index].insert(it);
        }
    }
    void add_into_bbl_index(deque<uint64_t> &current_lbr, uint64_t index, uint64_t bbl)
    {
        if(bbl_index.find(bbl)==bbl_index.end())
        {
            bbl_index[bbl]=unordered_map<uint64_t,set<uint64_t>>();
        }
        auto &next = bbl_index[bbl];
        next[index] = set<uint64_t>();
        for(auto it: current_lbr)
        {
            next[index].insert(it);
        }
    }
public:
    Classifier()
    {
        //do nothing
    }
    void add_new_filtered_bbls(uint64_t bbl_addr, double fan_out, vector<uint64_t> &good_indexes, uint64_t target)
    {
        if(filtered_bbls.find(bbl_addr)==filtered_bbls.end())
        {
            filtered_bbls[bbl_addr]=vector<MissInfo *>();
        }
        MissInfo *tmp = new MissInfo();
        tmp->fan_out = fan_out;
        for(auto j: good_indexes)
        {
            tmp->positive_indexes.insert(j);
        }
        tmp->target = target;
        filtered_bbls[bbl_addr].push_back(tmp);
    }
    void add_full_ordered_bbls(vector<uint64_t> &ordered_bbls,uint64_t lbr_capacity=LBR_CAPACITY)
    {
        bbl_index.clear();
        deque<uint64_t> _queue;
        _queue.clear();
        for(uint64_t i = 0; i< ordered_bbls.size(); i++)
        {
            uint64_t bbl_address = ordered_bbls[i];
            if(filtered_bbls.find(bbl_address)!=filtered_bbls.end())add_into_bbl_index(_queue,i,bbl_address);
            if(_queue.size()==lbr_capacity)
            {
                _queue.pop_front();
            }
            _queue.push_back(bbl_address);
        }
    }
    Classifier(string bbl_log_path)
    {
        string filtered_bbl_path = "/mnt/storage/takh/pgp/workloads/wordpress/analysis/tmp/f.2.27.200.1.txt";
        filtered_bbls.clear();
        FILE *fp = fopen(filtered_bbl_path.c_str(), "r");
        uint64_t bbl_addr;
        double fan_out;
        uint64_t size;
        while(fscanf(fp,"%" SCNu64 " %lf %" SCNu64 "",&bbl_addr,&fan_out,&size)!=EOF)
        {
            if(filtered_bbls.find(bbl_addr)==filtered_bbls.end())
            {
                filtered_bbls[bbl_addr]=vector<MissInfo *>();
            }
            MissInfo *tmp = new MissInfo();
            tmp->fan_out = fan_out;
            uint64_t j;
            for(uint64_t i =0;i<size; i++)
            {
                if(fscanf(fp," %" SCNu64 "",&j)==EOF)
                {
                    cerr<<"Error"<<endl;
                }
                tmp->positive_indexes.insert(j);
            }
            filtered_bbls[bbl_addr].push_back(tmp);
        }
        fclose(fp);
        //cout<<"Filtered bbl info read finished"<<endl;
        full_ordered_bbls.clear();
        bbl_index.clear();
        vector<string> raw_bbl_log_data;
        read_full_file(bbl_log_path, raw_bbl_log_data);
        deque<uint64_t> _queue;
        _queue.clear();
        for(uint64_t i = 0; i< raw_bbl_log_data.size(); i++)
        {
            string line = raw_bbl_log_data[i];
            boost::trim_if(line,boost::is_any_of(","));
            vector<string> parsed;
            boost::split(parsed,line,boost::is_any_of(",\n"),boost::token_compress_on);
            uint64_t bbl_address = string_to_u64(parsed[0]);
            full_ordered_bbls.push_back(bbl_address);
            parsed.clear();
            if(filtered_bbls.find(bbl_address)!=filtered_bbls.end())add_into_bbl_index(_queue);
            if(_queue.size()==LBR_CAPACITY)
            {
                _queue.pop_front();
            }
            _queue.push_back(bbl_address);
        }
        raw_bbl_log_data.clear();
        //cout<<full_ordered_bbls.size()<<endl;
    }
    void count_false_positives(FP_Result &result, set<uint64_t> &context, uint64_t candidate, set<uint64_t> &pos_ind, set<uint64_t> &all_ind)
    {
        for(int i=0;i<FP_SIZE; i++)
        {
            result.size[i]+=result.log_table[i];
        }

        for(auto index: all_ind)
        {
            if(pos_ind.find(index)==pos_ind.end())
            {
                if(includes(bbl_index[candidate][index].begin(),bbl_index[candidate][index].end(),context.begin(),context.end()))
                {
                    //do nothing
                }
                else
                {
                    for(int i=0;i<FP_SIZE; i++)
                    {
                        if(match(bbl_index[candidate][index],context,result.log_table[i]))
                        {
                            result.fp[i]++;
                        }
                        else
                        {
                            result.tn[i]++;
                        }
                    }
                }
            }
        }
    }
    Result find_context(uint64_t candidate, set<uint64_t> &pos_ind, set<uint64_t> &all_ind)
    {
        set<uint64_t> always_present_in_pos_ind;
        bool is_first = true;
        for(auto &it_pos_ind: pos_ind)
        {
            auto &current = bbl_index[candidate][it_pos_ind];
            if(is_first)
            {
                for(auto bbl: current)always_present_in_pos_ind.insert(bbl);
                is_first=false;
            }
            else
            {
                set<uint64_t> tmp;
                for(auto bbl: always_present_in_pos_ind)
                {
                    if(current.find(bbl)!=current.end())tmp.insert(bbl);
                }
                if(tmp.size()==0)
                {
                    always_present_in_pos_ind.clear();
                    return {false,pos_ind.size(),all_ind.size(),0};
                }
                always_present_in_pos_ind.clear();
                for(auto bbl: tmp)always_present_in_pos_ind.insert(bbl);
                tmp.clear();
            }
        }

        uint64_t all_freq=0;

        for(auto &it_all_ind: all_ind)
        {
            if(includes(bbl_index[candidate][it_all_ind].begin(),bbl_index[candidate][it_all_ind].end(),always_present_in_pos_ind.begin(),always_present_in_pos_ind.end()))
            {
                all_freq++;
            }
        }
        Result r;
        if(all_freq<all_ind.size())
        {
            //fan-out reduced
            r = {true,pos_ind.size(),all_freq,always_present_in_pos_ind.size()};
            for(auto j: always_present_in_pos_ind)
            {
                r.context.insert(j);
            }
        }
        else
        {
            r = {false,pos_ind.size(),all_ind.size(),0};
        }
        always_present_in_pos_ind.clear();
        return r;
    }
    void find_all_context(ofstream *out, uint64_t &current_running_count)
    {
        FP_Result fp_result;
        for(auto it_filtered_bbls: filtered_bbls)
        {
            auto &candidate = it_filtered_bbls.first;
            set<uint64_t> all_indexes;
            for(auto &it_bbl_index: bbl_index[candidate])
            {
                all_indexes.insert(it_bbl_index.first);
            }
            for(auto current_miss: it_filtered_bbls.second)
            {
                auto result = find_context(candidate,current_miss->positive_indexes,all_indexes);
                if(result.has_found)
                {
                    *out<<candidate<<" "<<result.context.size();
                    for(auto j: result.context)*out<<" "<<j;
                    *out<<" "<<current_miss->target<<endl;
                    current_running_count+=result.total_prefetch;
                    //count_false_positives(fp_result,result.context,candidate,current_miss->positive_indexes,all_indexes);
                    result.context.clear();
                }
                else
                {
                    *out<<candidate<<" "<<0<<" "<<current_miss->target<<endl;
                    current_running_count+=all_indexes.size();
                }
            }
            all_indexes.clear();
        }
        /*for(int i =0; i< FP_SIZE; i++)
        {
            cerr<<fp_result.log_table[i]<<","<<100.0*fp_result.fp[i]/(fp_result.fp[i]+fp_result.tn[i])<<","<<fp_result.size[i]<<endl;
        }*/
    }
    void find_all_context()
    {
        uint64_t prefetch_count[2][2]={0,0,0,0};
        uint64_t found[2]={0,0};
        //[0][0]->prev total
        //[0][1]->prev useful
        //[1][0]->new total
        //[1][1]->new useful
        for(auto it_filtered_bbls: filtered_bbls)
        {
            auto &candidate = it_filtered_bbls.first;
            set<uint64_t> all_indexes;
            for(auto &it_bbl_index: bbl_index[candidate])
            {
                all_indexes.insert(it_bbl_index.first);
            }
            for(auto current_miss: it_filtered_bbls.second)
            {
                //cout<<candidate<<" "<<current_miss->fan_out<<" "<<1.0*current_miss->positive_indexes.size()/all_indexes.size()<<endl;
                //cout<<"--------------------------"<<endl;
                prefetch_count[0][0]+=all_indexes.size();
                prefetch_count[0][1]+=current_miss->positive_indexes.size();
                auto result = find_context(candidate,current_miss->positive_indexes,all_indexes);
                if(result.has_found)
                {
                    //cout<<"--------------------------"<<endl;
                    //cout<<candidate<<" "<<current_miss->positive_indexes.size()<<" "<<all_indexes.size()<<" "<<result.total_prefetch<<" "<<result.nums<<endl;
                    prefetch_count[1][0]+=result.total_prefetch;
                    prefetch_count[1][1]+=result.useful_prefetch;
                    found[1]++;
                }
                else
                {
                    prefetch_count[1][0]+=all_indexes.size();
                    prefetch_count[1][1]+=current_miss->positive_indexes.size();
                    found[0]++;
                }
                //cout<<"--------------------------"<<endl;
                //cout<<candidate<<" "<<current_miss->fan_out<<" "<<current_miss->positive_indexes.size()<<" "<<all_indexes.size()<<" ";
                //negative_indexes.clear();
            }
            all_indexes.clear();
        }
        cout<<"Prev: "<<prefetch_count[0][1]<<" "<<prefetch_count[0][0]<<" "<<100.0*prefetch_count[0][1]/prefetch_count[0][0]<<endl;
        cout<<"New: "<<prefetch_count[1][1]<<" "<<prefetch_count[1][0]<<" "<<100.0*prefetch_count[1][1]/prefetch_count[1][0]<<endl;
        cout<<found[1]<<" "<<found[0]<<" "<<100.0*found[1]/(found[0]+found[1])<<endl;
    }
    ~Classifier()
    {
        full_ordered_bbls.clear();
        /*for(auto it_first: bbl_index)
        {
            auto &next = it_first.second;
            for(auto it_second: next)
            {
                it_second.second.clear();
            }
            next.clear();
        }*/
        bbl_index.clear();
        filtered_bbls.clear();
    }
};

#endif //CLASSIFIER_H_