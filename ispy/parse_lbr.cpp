#include <bits/stdc++.h>
#include <zlib.h>
#include <boost/algorithm/string.hpp>

using namespace std;

uint64_t l3_access_cycle_count = 38;
uint64_t total_dyn_ins_count = 163607974;
double dyn_ins_count_inc_ratio=0.013;

#define BUFFER_SIZE 2048
#define FANIN 100
#define FANIN_THRESHOLD 10


uint64_t find_prefetch_index(vector<pair<string,uint64_t>> &current_lb_record)
{
    uint64_t running_cycle_count = 0;
    for(uint64_t i = 0; i < current_lb_record.size(); i++)
    {
        running_cycle_count+=current_lb_record[i].second;
        if(running_cycle_count>=l3_access_cycle_count)return i;
    }
    return current_lb_record.size()-1;
}

bool find_prefetch_index_for_multiple_samples_with_fanin(string missed_pc, vector<vector<pair<string,uint64_t>>> &current_lb_record_list, vector<string> &result)
{
    set<string> candidates_array[FANIN];
    uint64_t candidates_array_count[FANIN];
    for(uint64_t i=0;i<FANIN;i++)candidates_array_count[i]=0;
    uint64_t current_index = 0;
    for(int i = 0; i< current_lb_record_list.size(); i++)
    {
        uint64_t start_index = find_prefetch_index(current_lb_record_list[i]);
        if(start_index==current_lb_record_list[i].size())return false;
        start_index+=1;
        bool not_found = true;
        int j = 0;
        for(j=0; j< current_index; j++)
        {
            set<string> &candidates = candidates_array[j];
            
            set<string> tmp;
            for(int k = start_index; k<current_lb_record_list[i].size();k++)
            {
                string pc = current_lb_record_list[i][k].first;
                if(pc!=missed_pc)
                {
                    if(candidates.find(pc)!=candidates.end())
                    {
                        tmp.insert(pc);
                    }
                }
            }
            if(tmp.size()!=0)
            {
                not_found = false;
                candidates.clear();
                for(auto it: tmp)
                {
                    candidates.insert(it);
                }
                candidates_array_count[j]++;
                break;
            }
        }
        if(not_found)
        {
            current_index+=1;
            if(current_index>=FANIN)return false;
            for(int k = start_index; k< current_lb_record_list[i].size();k++)
            {
                string pc = current_lb_record_list[i][k].first;
                if(pc!=missed_pc)
                {
                    candidates_array[current_index].insert(pc);
                }
            }
            if(candidates_array[current_index].size()<1)return false;
        }
        else
        {
            //j contains the first found set
        }
    }
    
    vector<pair<uint64_t,string>> sorted_result;
    for(int j=0;j< current_index; j++)
    {
        set<string> &candidates = candidates_array[j];
        if(candidates.size()<1)continue;
        vector<pair<int,string>> sorted_candidates;
        for(auto it: candidates)
        {
            int total_distance = 0;
            for(int i = 0; i< current_lb_record_list.size();i++)
            {
                for(int k=0;k<current_lb_record_list[i].size();k++)
                {
                    if(current_lb_record_list[i][k].first == it)
                    {
                        total_distance+=k;
                        break;
                    }
                }
            }
            sorted_candidates.push_back(make_pair(total_distance,it));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        //for(int i = 0; i< sorted_candidates.size(); i++)cout<<sorted_candidates[i].second<<";"<<sorted_candidates[i].first<<",";
        //cout<<sorted_candidates[0].second<<endl;
        sorted_result.push_back(make_pair(candidates_array_count[j],sorted_candidates[0].second));
    }
    sort(sorted_result.begin(),sorted_result.end());
    reverse(sorted_result.begin(),sorted_result.end());
    result.clear();
    for(int j=0;j<sorted_result.size();j++)
    {
        result.push_back(sorted_result[j].second);
        if(j>=FANIN_THRESHOLD-1)break;
    }
    return true;
}

bool find_prefetch_index_for_multiple_samples(string missed_pc, vector<vector<pair<string,uint64_t>>> &current_lb_record_list, string &result)
{
    result="";
    set<string> candidates;
    uint64_t start_index = find_prefetch_index(current_lb_record_list[0]);
    start_index+=1;
    if(start_index==current_lb_record_list[0].size())
    {
        //cout<<"Not found\n";
        return false;
    }
    for(int i = start_index; i<current_lb_record_list[0].size();i++)
    {
        string pc = current_lb_record_list[0][i].first;
        if(pc!=missed_pc)
        {
            candidates.insert(pc);
        }
    }
    bool found = true;
    for(int i = 1; i< current_lb_record_list.size(); i++)
    {
        set<string> tmp;
        start_index = find_prefetch_index(current_lb_record_list[i]);
        start_index+=1;
        if(start_index==current_lb_record_list[i].size())
        {
            found = false;
            break;
        }
        for(int j = start_index; j<current_lb_record_list[i].size();j++)
        {
            string pc = current_lb_record_list[i][j].first;
            if(pc!=missed_pc)
            {
                if(candidates.find(pc)!=candidates.end())
                {
                    tmp.insert(pc);
                }
            }
        }
        if(tmp.size()==0)
        {
            found = false;
            break;
        }
        candidates.clear();
        for(auto it: tmp)
        {
            candidates.insert(it);
        }
    }
    if(found)
    {
        vector<pair<int,string>> sorted_candidates;
        for(auto it: candidates)
        {
            int total_distance = 0;
            for(int i = 0; i< current_lb_record_list.size();i++)
            {
                for(int j=0;j<current_lb_record_list[i].size();j++)
                {
                    if(current_lb_record_list[i][j].first == it)
                    {
                        total_distance+=j;
                        break;
                    }
                }
            }
            sorted_candidates.push_back(make_pair(total_distance,it));
        }
        sort(sorted_candidates.begin(), sorted_candidates.end());
        //for(int i = 0; i< sorted_candidates.size(); i++)cout<<sorted_candidates[i].second<<";"<<sorted_candidates[i].first<<",";
        //cout<<sorted_candidates[0].second<<endl;
        result = sorted_candidates[0].second;
        return true;
    }
    else
    {
        //cout<<"Not found\n";
        return false;
    }
}

uint64_t string_to_u64(string a)
{
    istringstream iss(a);
    uint64_t value;
    iss>>value;
    return value;
}

void check_pc_miss_same_cache_line(string cache_line_address, string pc)
{
    assert((string_to_u64(pc)>>6)==string_to_u64(cache_line_address));
}

int main(int argc, char *argv[])
{
    if(argc<2)
    {
        cerr << "Usage ./exec data_file_path\n";
        return -1;
    }
    gzFile data_file = gzopen(argv[1], "rb");
    if(!data_file)
    {
        cerr << "Invalid data_file_path\n";
        return -1;
    }
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    if(gzbuffer(data_file, BUFFER_SIZE*128) == -1)
    {
        cerr << "GZ Input file buffer set unsuccessfull\n";
        return -1;
    }
    string line;
    vector<string> parsed;
    string cache_line_address,pc;
    unordered_map<string,vector<vector<pair<string,uint64_t>>>> profile;
    unordered_map<string,uint64_t> miss_counts;
    while( gzgets(data_file,buffer,BUFFER_SIZE) != Z_NULL )
    {
        line=buffer;
        boost::trim_if(line,boost::is_any_of(",\n"));
        boost::split(parsed,line,boost::is_any_of(",\n"),boost::token_compress_on);
        //for(int i =0;i<parsed.size();i++)cerr<<parsed[i]<<",";
        //cerr<<endl;
        if(parsed.size()<3)
        {
            cerr << "LBR data contains less than 3 branch records\n";
            return -1;
        }
        cache_line_address = parsed[0];
        pc = parsed[1];
        check_pc_miss_same_cache_line(cache_line_address, pc);
        vector<string> current_record;
        vector<pair<string,uint64_t>> current_lb_record;
        for(int i=2;i<parsed.size();i++)
        {
            //cerr<<parsed[i]<<endl;
            boost::split(current_record,parsed[i],boost::is_any_of(";"),boost::token_compress_on);
            if(current_record.size() != 2)
            {
                cerr << "LB-record does not contain exactly 2 entries\n";
                return -1;
            }
            current_lb_record.push_back(make_pair(current_record[0],stoi(current_record[1])));
        }
        if(profile.find(pc)==profile.end())
        {
            profile[pc]=vector<vector<pair<string,uint64_t>>>();
            miss_counts[pc]=0;
        }
        profile[pc].push_back(current_lb_record);
        miss_counts[pc]+=1;
    }
    vector<pair<uint64_t,string>> sorted_miss_pcs;
    for(auto it: miss_counts)
    {
        sorted_miss_pcs.push_back(make_pair(it.second, it.first));
    }
    sort(sorted_miss_pcs.begin(), sorted_miss_pcs.end());
    reverse(sorted_miss_pcs.begin(), sorted_miss_pcs.end());
    uint64_t permissible_prefetch_count = dyn_ins_count_inc_ratio * total_dyn_ins_count;
    uint64_t current_running_count = 0;
    
    unordered_map<string,set<string>> bbl_address_prefetch_map;
    
    for(int i=0;i<sorted_miss_pcs.size();i++)
    {
        //cout<<sorted_miss_pcs[i].second<<","<<sorted_miss_pcs[i].first<<",";
        string pc = sorted_miss_pcs[i].second;
        if(profile[pc].size()>1)
        {
            /*string result;
            bool is_found = find_prefetch_index_for_multiple_samples(pc, profile[pc],result);
            if(is_found)cout<<result;
            else
            {*/
            vector<string> results;
            bool is_found = find_prefetch_index_for_multiple_samples_with_fanin(pc, profile[pc],results);
            if(is_found)
            {
                //for(int j=0;j<results.size();j++)cout<<results[j]<<",";
                for(int j = 0;j<results.size();j++)
                {
                    string bbl_address = results[j];
                    if(bbl_address_prefetch_map.find(bbl_address)==bbl_address_prefetch_map.end())
                    {
                        bbl_address_prefetch_map[bbl_address]=set<string>();
                    }
                    bbl_address_prefetch_map[bbl_address].insert(pc);
                }
            }
            //}
        }
        else if(profile[pc].size()==1)
        {
            uint64_t index = find_prefetch_index(profile[pc][0]);
            index+=1;
            if(index==profile[pc][0].size())
            {
                //cerr<<"No good prefetch position found\n";
            }
            else
            {
                //cout<<profile[pc][0][index].first<<"->"<<profile[pc][0][index-1].first<<"\n";
                //cout<<profile[pc][0][index].first;
                
                string bbl_address = profile[pc][0][index].first;
                if(bbl_address_prefetch_map.find(bbl_address)==bbl_address_prefetch_map.end())
                {
                    bbl_address_prefetch_map[bbl_address]=set<string>();
                }
                bbl_address_prefetch_map[bbl_address].insert(pc);
            }
        }
        else
        {
            cerr<<"Miss without lbr profile\n";
            return -1;
        }
        //cout<<endl;
        
        current_running_count+=sorted_miss_pcs[i].first;
        if(current_running_count>=permissible_prefetch_count)break;
    }
    
    for(auto it:bbl_address_prefetch_map)
    {
        cout<<it.first<<" "<<it.second.size();
        for(auto set_it: it.second)
        {
            cout<<" "<<set_it;
        }
        cout<<endl;
    }
}
