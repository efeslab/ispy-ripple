#ifndef SETTINGS_H_
#define SETTINGS_H_
#include <bits/stdc++.h>

#include "config.h"
#include "convert.h"

using namespace std;

// To select between different modes of our approach
enum our_mode_selector { NA, ALL, ONLY_COALESCE, ONLY_CONTEXT };

class Settings {
public:
  uint64_t min_distance_cycle_count;
  uint64_t max_distance_cycle_count;
  uint64_t total_dyn_ins_count;
  uint64_t total_cycles;
  // double dyn_ins_count_inc_ratio;
  double fan_out_ratio;
  uint64_t max_step_ahead;
  uint64_t max_prefetch_length;
  double min_ratio_percentage;
  uint64_t multiline_mode;
  double ipc;
  uint64_t insert_as_many_as_possible;
  bool logging;
  bool remove_lbr_cycle;
  uint64_t context_size;
  our_mode_selector our_mode;
  uint64_t fan_in_distance;
  uint64_t best_probability;
  uint8_t prefetch_setup;
  Settings(MyConfigWrapper &conf) {
    best_probability = string_to_u64(conf.lookup("best_probability", "50"));
    prefetch_setup = string_to_u64(conf.lookup("prefetch_setup", "0"));
    min_distance_cycle_count =
        string_to_u64(conf.lookup("min_distance_cycle_count", "40"));
    max_distance_cycle_count =
        string_to_u64(conf.lookup("max_distance_cycle_count", "200"));
    total_dyn_ins_count =
        string_to_u64(conf.lookup("total_dyn_ins_count", "163607974"));
    total_cycles = string_to_u64(conf.lookup("total_cycles", "225464000"));
    // dyn_ins_count_inc_ratio =
    // string_to_double(conf.lookup("dyn_ins_count_inc_ratio","0.01"));
    fan_out_ratio = string_to_double(conf.lookup("fan_out_ratio", "0.5"));
    max_step_ahead = string_to_u64(conf.lookup("max_step_ahead", "10"));
    max_prefetch_length =
        string_to_u64(conf.lookup("max_prefetch_length", "8"));
    min_ratio_percentage =
        string_to_double(conf.lookup("min_ratio_percentage", "0.99"));
    multiline_mode = string_to_u64(conf.lookup("multiline_mode", "1"));
    context_size = string_to_u64(conf.lookup("context_size", "32"));

    fan_in_distance = string_to_u64(conf.lookup("fan_in_distance", "10"));

    if (multiline_mode == 2) {
      string only_coalesce_or_context = conf.lookup("our_mode", "None");
      if (only_coalesce_or_context == "ALL") {
        our_mode = ALL;
      } else if (only_coalesce_or_context == "CLSC") {
        our_mode = ONLY_COALESCE;
      } else if (only_coalesce_or_context == "CNTXT") {
        our_mode = ONLY_CONTEXT;
      } else {
        our_mode = ALL;
      }
    } else {
      our_mode = NA;
    }

    insert_as_many_as_possible =
        string_to_u64(conf.lookup("insert_as_many_as_possible", "0"));
    logging = false;
    remove_lbr_cycle = true;

    ipc = ((1.0 * total_dyn_ins_count) / total_cycles);
  }
  uint64_t get_min_distance() {
    if (multiline_mode == 1 ||
        remove_lbr_cycle == true) // ASMDB static implementation
    {
      return min_distance_cycle_count * ipc;
    } else // if(multiline_mode==2) //OUR
    {
      return min_distance_cycle_count;
    }
  }
  uint64_t get_max_distance() {
    if (multiline_mode == 1 ||
        remove_lbr_cycle == true) // ASMDB static implementation
    {
      return max_distance_cycle_count * ipc;
    } else // if(multiline_mode==2) //OUR
    {
      return max_distance_cycle_count;
    }
  }
};
#endif // SETTINGS_H_
