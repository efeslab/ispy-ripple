#ifndef LOG_H_
#define LOG_H_
#include <iostream>
#include <cstdlib>
#include <string>

using namespace std;

void panic(string error_string)
{
    cerr<<error_string<<endl;
    exit(EXIT_FAILURE);
}

#endif //LOG_H_
