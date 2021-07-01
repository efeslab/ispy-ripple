#include <bits/stdc++.h>
#include <zlib.h>
#include <boost/algorithm/string.hpp>
#include <string>

#include "log.h"
#include "config.h"
#include "settings.h"
#include "cfg.h"
#include "lbr_miss_sample.h"

using namespace std;

int main(int argc, char *argv[])
{
    if(argc<2)
    {
        panic("Usage ./exec config_cfg_file_path");
    }
    MyConfigWrapper config(argv[1]);
    Settings sim_settings(config);
    CFG dynamic_cfg(config, sim_settings);
    Miss_Profile profile(config, sim_settings, dynamic_cfg, true);
    return 0;
}
