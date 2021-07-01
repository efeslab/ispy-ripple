#include <bits/stdc++.h>

using namespace std;

#include "bloom_filter.hpp"

int main(void)
{
    for(int i=4;i<8;i*=2)
    {
        for(int j=1;j<=100;j++)
        {
            bloom_parameters parameters;
            //parameters.maximum_size=16;
            parameters.projected_element_count = i;
            parameters.maximum_number_of_hashes = 4;
            parameters.minimum_number_of_hashes = 4;
            parameters.false_positive_probability = 1.0*j/100.0;
            //parameters.maximum_size=i;
            if (!parameters)
            {
                std::cout << "Error - Invalid set of bloom filter parameters!" << std::endl;
            }
            else
            {
                parameters.compute_optimal_parameters();    
                cout<<j<<" "<<parameters.optimal_parameters.number_of_hashes<<" "<<parameters.optimal_parameters.table_size<<endl;
            }
        }
    }
}