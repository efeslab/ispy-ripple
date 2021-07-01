#include <bits/stdc++.h>

using namespace std;

uint64_t max_step_ahead = 10;
uint64_t max_prefetch_length = 8;
double min_ratio_percentage = 90.0;

int measure_prefetch_length(uint64_t current, uint64_t next)
{
    return (current>>6)-(next>>6);
}
unsigned int my_abs(int a)
{
    if(a<0)return -a;
    return a;
}

void analyze(vector<uint64_t> &miss_profile)
{
    unordered_map<uint64_t,uint64_t> counts;
    unordered_map<uint64_t,unordered_map<int,uint64_t>> neighbors;
    for(uint64_t i = 0; i < miss_profile.size(); i++)
    {
        uint64_t missed_pc = miss_profile[i];
        if(counts.find(missed_pc)==counts.end())
        {
            counts[missed_pc]=1;
            neighbors[missed_pc]=unordered_map<int,uint64_t>();
        }
        else counts[missed_pc]+=1;
        for(uint64_t j = 1; j<= max_step_ahead; j++)
        {
            if(i+j>miss_profile.size())break;
            int prefetch_length = measure_prefetch_length(missed_pc, miss_profile[i+j]);
            if(my_abs(prefetch_length)<=max_prefetch_length)
            {
                if(prefetch_length==0)continue;
                if(neighbors[missed_pc].find(prefetch_length)==neighbors[missed_pc].end())
                {
                    neighbors[missed_pc][prefetch_length]=1;
                }
                else
                {
                    neighbors[missed_pc][prefetch_length]+=1;
                }
            }
        }
    }
    vector<pair<uint64_t,uint64_t>> sorted_missed_pcs;
    for(auto it: counts)
    {
        sorted_missed_pcs.push_back(make_pair(it.second,it.first));
    }
    sort(sorted_missed_pcs.begin(), sorted_missed_pcs.end());
    reverse(sorted_missed_pcs.begin(), sorted_missed_pcs.end());
    cout<<"Correlation threshold (%), "<<min_ratio_percentage<<", "<<endl;
    cout<<"Maximum step ahead in miss distance, "<<max_step_ahead<<", "<<endl;
    cout<<"Maximum distance in number of cache lines (64 bytes), "<<max_prefetch_length<<", "<<endl;
    cout<<"Program counter where miss happens, Total number of misses, Correlated neighbor cache line count, cache line distance from neighbor, miss correlation"<<endl;
    for(uint64_t i = 0; i < sorted_missed_pcs.size(); i++) 
    {
        uint64_t current_pc = sorted_missed_pcs[i].second;
        cout<<current_pc<<", "<<sorted_missed_pcs[i].first<<", ";
        uint64_t tmp_count = 0;
        for(auto it: neighbors[current_pc])
        {
            double ratio_percentage = 100.0*it.second/sorted_missed_pcs[i].first;
            if(ratio_percentage>=min_ratio_percentage)tmp_count++;
        }
        cout<<tmp_count<<", ";
        for(auto it: neighbors[current_pc])
        {
            double ratio_percentage = 100.0*it.second/sorted_missed_pcs[i].first;
            if(ratio_percentage>=min_ratio_percentage)cout<<it.first<<", "<<ratio_percentage<<", ";
        }
        cout<<endl;
    }
}

int main(int argc, char *argv[])
{
    if(argc<2)
    {
        cerr << "Usage ./exec data_file_path\n";
        return -1;
    }
    ifstream data_file(argv[1]);
    vector<uint64_t> miss_profile;
    if(data_file.is_open())
    {
        uint64_t tmp;
        while(data_file>>tmp)miss_profile.push_back(tmp);
    }
    else
    {
        cerr << "Invalid data_file_path\n";
        return -1;
    }
    //cout << miss_profile.size() << endl;
    analyze(miss_profile);
    return 0;
}
