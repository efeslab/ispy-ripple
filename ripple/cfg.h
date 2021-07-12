#ifndef CFG_H_
#define CFG_H_
#include <bits/stdc++.h>

using namespace std;

#include <boost/algorithm/string.hpp>

#include "config.h"
#include "convert.h"
#include "gz_reader.h"
#include "log.h"
#include "settings.h"

class BBL {
public:
  uint64_t total_count;
  uint32_t instrs;
  uint32_t bytes;
  uint64_t average_cycles;
  BBL(uint32_t instr_count, uint32_t byte_size) {
    total_count = 0;
    instrs = instr_count;
    bytes = byte_size;
    average_cycles = instr_count;
  }
  void increment() { total_count += 1; }
  uint64_t get_count() { return total_count; }
};

class CFG {
private:
  unordered_map<uint64_t, BBL *> bbl_infos;
  set<uint64_t> self_modifying_bbls;
  unordered_map<uint64_t, pair<uint64_t, uint64_t>> self_modifying_inst_bytes;
  vector<uint64_t> full_ordered_bbls;
  Settings *sim_settings;
  unordered_map<uint64_t, vector<uint64_t>> bbl_locations_in_the_ordered_log;
  uint64_t static_size;
  uint64_t static_count;
  uint64_t dynamic_size;
  uint64_t dynamic_count;
  unordered_map<uint64_t, set<uint64_t>> accessed_soon;
  unordered_map<uint64_t, uint64_t> btb;

public:
  vector<uint64_t> &get_full_ordered_bbls() { return full_ordered_bbls; }
  void init_btb() {
    btb.clear();
    unordered_map<uint64_t, unordered_map<uint64_t, uint64_t>> next_bbl_counter;
    for (uint64_t i = 1; i < full_ordered_bbls.size(); i++) {
      const auto previous = full_ordered_bbls[i - 1];
      const auto current = full_ordered_bbls[i];
      if (next_bbl_counter.count(previous) == 0) {
        next_bbl_counter[previous] = unordered_map<uint64_t, uint64_t>();
      }
      if (next_bbl_counter[previous].count(current) == 0) {
        next_bbl_counter[previous][current] = 1;
      } else {
        next_bbl_counter[previous][current] += 1;
      }
    }
    for (const auto &KV : next_bbl_counter) {
      uint64_t next;
      uint64_t next_counter;
      bool is_first = true;
      for (const auto &it : KV.second) {
        if (is_first) {
          is_first = false;
          next = it.first;
          next_counter = it.second;
        } else if (it.second > next_counter) {
          next = it.first;
          next_counter = it.second;
        }
      }
      if (!is_first) {
        btb[KV.first] = next >> 6;
      }
    }
  }
  bool get_btb_target(uint64_t src, uint64_t &target) {
    if (btb.count(src) > 0) {
      target = btb[src];
      return true;
    } else {
      return false;
    }
  }
  uint64_t get_bbl_size(uint64_t bbl_pc) {
    assert(bbl_infos.count(bbl_pc) > 0);
    return bbl_infos[bbl_pc]->bytes;
  }
  uint64_t get_bbl_count(uint64_t bbl_pc) {
    assert(bbl_infos.count(bbl_pc) > 0);
    return bbl_infos[bbl_pc]->total_count;
  }
  bool is_accessed_soon(uint64_t bbl, uint64_t cl_target) {
    return accessed_soon.count(bbl) && accessed_soon[bbl].count(cl_target);
  }
  bool is_self_modifying(uint64_t bbl_address) {
    return self_modifying_bbls.count(bbl_address);
  }
  uint64_t get_static_size() { return static_size; }
  uint64_t get_static_count() { return static_count; }
  uint64_t get_dynamic_size() { return dynamic_size; }
  uint64_t get_dynamic_count() { return dynamic_count; }
  CFG(MyConfigWrapper &conf, Settings &settings) {
    sim_settings = &settings;
    string self_modifying_bbl_info_path =
        conf.lookup("bbl_info_self_modifying", "/tmp/");
    static_count = 0;
    static_size = 0;
    if (self_modifying_bbl_info_path == "/tmp/") {
      // do nothing
      // since self modifying bbls are optional only for jitted codes
    } else {
      vector<string> raw_self_modifying_bbl_info_data;
      read_full_file(self_modifying_bbl_info_path,
                     raw_self_modifying_bbl_info_data);
      for (uint64_t i = 0; i < raw_self_modifying_bbl_info_data.size(); i++) {
        string line = raw_self_modifying_bbl_info_data[i];
        boost::trim_if(line, boost::is_any_of(","));
        vector<string> parsed;
        boost::split(parsed, line, boost::is_any_of(",\n"),
                     boost::token_compress_on);
        if (parsed.size() != 3) {
          panic("A line on the self-modifying BBL info file does not have "
                "exactly three tuples");
        }
        uint64_t bbl_address = string_to_u64(parsed[0]);
        uint64_t inst = string_to_u64(parsed[1]);
        static_count += inst;
        uint64_t bytes = string_to_u64(parsed[2]);
        static_size += bytes;
        self_modifying_bbls.insert(bbl_address);
        if (self_modifying_inst_bytes.count(bbl_address) > 0) {
          if (self_modifying_inst_bytes[bbl_address].first < inst) {
            self_modifying_inst_bytes[bbl_address].first = inst;
          }
          if (self_modifying_inst_bytes[bbl_address].second < bytes) {
            self_modifying_inst_bytes[bbl_address].second = bytes;
          }
        } else {
          self_modifying_inst_bytes[bbl_address] = make_pair(inst, bytes);
        }
      }
      raw_self_modifying_bbl_info_data.clear();
    }
    string bbl_info_path = conf.lookup("bbl_info", "/tmp/");
    if (bbl_info_path == "/tmp/") {
      panic("BBL Info file is not present in the config");
    }
    vector<string> raw_bbl_info_data;
    read_full_file(bbl_info_path, raw_bbl_info_data);
    for (uint64_t i = 0; i < raw_bbl_info_data.size(); i++) {
      string line = raw_bbl_info_data[i];
      boost::trim_if(line, boost::is_any_of(","));
      vector<string> parsed;
      boost::split(parsed, line, boost::is_any_of(",\n"),
                   boost::token_compress_on);
      if (parsed.size() != 3) {
        panic("A line on the BBL info file does not have exactly three tuples");
      }
      uint64_t bbl_address = string_to_u64(parsed[0]);
      uint32_t instrs = string_to_u32(parsed[1]);
      static_count += instrs;
      uint32_t bytes = string_to_u32(parsed[2]);
      static_size += bytes;
      if (bbl_infos.find(bbl_address) == bbl_infos.end()) {
        bbl_infos[bbl_address] = new BBL(instrs, bytes);
      } else {
        panic("BBL info file includes multiple info entries for the same BBL "
              "start address");
      }
      parsed.clear();
    }
    raw_bbl_info_data.clear();
    full_ordered_bbls.clear();
    string bbl_log_path = conf.lookup("bbl_log", "/tmp/");
    if (bbl_log_path == "/tmp/") {
      panic("BBL Log file is not present in the config");
    }
    vector<string> raw_bbl_log_data;
    read_full_file(bbl_log_path, raw_bbl_log_data);
    uint64_t prev[] = {0, 0};
    unordered_map<uint64_t, pair<uint64_t, uint64_t>> sum_counts;

    deque<uint64_t> last_1000_bbls;

    dynamic_count = 0;
    dynamic_size = 0;

    for (uint64_t i = 0; i < raw_bbl_log_data.size(); i++) {
      string line = raw_bbl_log_data[i];
      boost::trim_if(line, boost::is_any_of(","));
      vector<string> parsed;
      boost::split(parsed, line, boost::is_any_of(",\n"),
                   boost::token_compress_on);
      if (parsed.size() != 2) {
        panic("A line on the BBL log file does not have exactly two tuples");
      }
      uint64_t bbl_address = string_to_u64(parsed[0]);
      uint64_t cycle = string_to_u64(parsed[1]);
      if (prev[0] != 0) {
        if (sum_counts.find(prev[0]) != sum_counts.end()) {
          sum_counts[prev[0]].first += cycle;
          sum_counts[prev[0]].second++;
        } else {
          sum_counts[prev[0]] = make_pair(cycle, 1);
        }
      }
      prev[0] = prev[1];
      prev[1] = bbl_address;
      full_ordered_bbls.push_back(bbl_address);
      if (is_self_modifying(bbl_address)) {
        dynamic_count += self_modifying_inst_bytes[bbl_address].first;
        dynamic_size += self_modifying_inst_bytes[bbl_address].second;
      } else {
        dynamic_count += bbl_infos[bbl_address]->instrs;
        dynamic_size += bbl_infos[bbl_address]->bytes;
      }
      parsed.clear();
      uint64_t my_cache_line = bbl_address >> 6;
      for (const auto &prev_bbls : last_1000_bbls) {
        accessed_soon[prev_bbls].insert(my_cache_line);
      }
      if (last_1000_bbls.size() == settings.fan_in_distance) {
        last_1000_bbls.pop_front();
      }
      last_1000_bbls.push_back(bbl_address);
    }
    raw_bbl_log_data.clear();
    last_1000_bbls.clear();

    for (auto it : sum_counts) {
      uint64_t bbl_address = it.first;
      uint64_t sum = it.second.first;
      uint64_t count = it.second.second;
      if (bbl_infos.find(bbl_address) == bbl_infos.end()) {
        panic(
            "BBL log file includes a BBL that is not present in BBL info file");
      } else {
        bbl_infos[bbl_address]->average_cycles = 1 + ((sum - 1) / count);
        bbl_infos[bbl_address]->total_count = count;
      }
    }
    sum_counts.clear();
    init_btb();
  }
};
#endif // CFG_H_