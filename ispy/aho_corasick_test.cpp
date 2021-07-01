#include <bits/stdc++.h>
#include <boost/dynamic_bitset.hpp>
#include "aho_corasick.hpp"

namespace ac = aho_corasick;
using utrie = ac::utrie;
using namespace std;

int main(void)
{
    basic_string<uint64_t> all = {0,1,2,3,0,1,2,0,1,0};
    vector<basic_string<uint64_t>> tests = {{0,1,2,3},{0,1,2},{0,1},{0},{3},{4},{0}};
    vector<uint64_t> test_counts;
    utrie t;
    for(auto & pattern: tests)
    {
        t.insert(pattern);
        test_counts.push_back(0);
    }
    auto matches = t.parse_text(all);
    cout<<matches.size()<<endl;
    for(auto match: matches)
    {
        /*cout<<endl;
        cout<<match.get_index()<<endl;
        cout<<match.get_start()<<","<<match.get_end()<<endl;
        for(auto it: match.get_keyword())cout<<it<<",";
        cout<<endl;*/
        test_counts[match.get_index()]++;
    }
    for(uint64_t i =0; i<tests.size(); i++)
    {
        for(auto it: tests[i])cout<<it<<",";
        cout<<test_counts[i]<<endl;
    }
    boost::dynamic_bitset<> reference(4);
    vector<boost::dynamic_bitset<>> test_bits;
    test_bits.push_back(boost::dynamic_bitset<>(4));
    test_bits.push_back(boost::dynamic_bitset<>(4));
    test_bits.push_back(boost::dynamic_bitset<>(4));
    test_bits.push_back(boost::dynamic_bitset<>(4));
    test_bits.push_back(boost::dynamic_bitset<>(4));
    test_bits[0][2]=test_bits[0][1]=1;
    test_bits[1][1]=test_bits[1][0]=1;
    test_bits[2][2]=test_bits[2][0]=1;
    test_bits[3][3]=test_bits[3][1]=test_bits[3][0]=1;
    test_bits[4][3]=test_bits[4][2]=test_bits[4][0]=1;
    for(auto &it : test_bits)
    {
        cout<<it<<",";
        it = (~reference)&(it);
        cout<<it.count()<<",";
        reference|=it;
        cout<<reference<<endl;
        if(reference.count()==4)break;
    }
}