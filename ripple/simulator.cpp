#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>
#include <string>
#include <zlib.h>

#include "cfg.h"
#include "config.h"
#include "harmony.h"
#include "log.h"
#include "settings.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    panic("Usage ./exec config_cfg_file_path");
  }
  MyConfigWrapper config(argv[1]);
  Settings sim_settings(config);
  CFG dynamic_cfg(config, sim_settings);
  HARMONY harmony(dynamic_cfg, sim_settings.best_probability,
                  sim_settings.prefetch_setup);
  return 0;
}