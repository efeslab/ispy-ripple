#ifndef HASH_H_
#define HASH_H_
#include <bits/stdc++.h>
#include <boost/dynamic_bitset.hpp>

using namespace std;

#include "CRC.h"
#include "bloom_filter.hpp"

uint64_t get_hash(uint64_t key)
{
    unsigned char value[sizeof(key)];
    std::memcpy(value,&key,sizeof(key));
    return CRC::Calculate(value, sizeof(value), CRC::CRC_32());
}

void compute_bitmask(set<uint64_t> &context, boost::dynamic_bitset<> &result)
{
    auto hash_size = result.size();
    result.reset();
    for(auto i: context)
    {
        result[get_hash(i)%hash_size]=1;
    }
}

bool match(set<uint64_t> &dynamic_context, set<uint64_t> &static_context, uint64_t size)
{
    /*boost::dynamic_bitset<> dynamic_hash(size);
    compute_bitmask(dynamic_context, dynamic_hash);
    boost::dynamic_bitset<> static_hash(size);
    compute_bitmask(static_context, static_hash);
    for(uint64_t i=0; i<static_hash.size(); i++)
    {
        if(static_hash[i])
        {
            if(dynamic_hash[i]==false)return false;
        }
    }
    return true;*/
    bloom_parameters parameters;
    parameters.projected_element_count = 32;
    parameters.false_positive_probability = 0.05;
    parameters.maximum_size=size;
    parameters.compute_optimal_parameters();
    bloom_filter filter(parameters);
    char arr[1];
    for(auto i:dynamic_context)
    {
        arr[0]=i%256;
        filter.insert(arr);
    }
    for(auto j:static_context)
    {
        arr[0]=j%256;
        if(!filter.contains(arr))return false;
    }
    return true;
}

#endif //HASH_H_