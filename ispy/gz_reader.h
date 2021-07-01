#ifndef GZ_READER_H_
#define GZ_READER_H_
#include <zlib.h>
#include <bits/stdc++.h>
using namespace std;
#include "log.h"
#include <boost/algorithm/string.hpp>

#define BUFFER_SIZE 2048

void read_full_file(string file_path, vector<string> &data_destination)
{
    gzFile raw_file = gzopen(file_path.c_str(), "rb");
    if(!raw_file)panic("Invalid GZ File");
    char buffer[BUFFER_SIZE];
    data_destination.clear();
    memset(buffer, 0, BUFFER_SIZE);
    string line;
    while(gzgets(raw_file,buffer,BUFFER_SIZE) != Z_NULL )
    {
        line=buffer;
        boost::trim_if(line, boost::is_any_of("\n"));
        data_destination.push_back(line);
    }
}
#endif //GZ_READER_H_
