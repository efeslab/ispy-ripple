#ifndef CONVERT_H_
#define CONVERT_H_
#include <bits/stdc++.h>
using namespace std;

uint64_t string_to_u64(string tmp)
{
    istringstream iss(tmp);
    uint64_t value;
    iss>>value;
    return value;
}
uint32_t string_to_u32(string tmp)
{
    istringstream iss(tmp);
    uint32_t value;
    iss>>value;
    return value;
}
double string_to_double(string tmp)
{
    istringstream iss(tmp);
    double value;
    iss>>value;
    return value;
}
#endif //CONVERT_H_
