#ifndef HARMONY_H_
#define HARMONY_H_
#include "cfg.h"
#include <bits/stdc++.h>
#include <boost/functional/hash.hpp>

using namespace std;

// 0-> no prefetch, 1-> nextline, 2-> FDIP
uint8_t prefetch_setup = 2;
bool no_design_space_explore = false;
bool measure_coverage_accuracy = true;
bool is_plru_random = false;
bool is_ghrp_plru = true;
// Now trying all contexts with length 1, 2, 4, 8, 16, 32, 64
uint8_t variable_pc_history[] = {1, 2, 4, 8, 16, 32, 64};
uint8_t pc_history = 64;

struct container_hash {
    std::size_t operator()(const vector<uint64_t> &c) const {
        return boost::hash_range(c.begin(), c.end());
    }
};

class OPTgen {
private:
  unordered_map<uint64_t, set<uint64_t>> future_accesses;
  unordered_map<uint64_t, set<uint64_t>> future_prefetches;
  set<uint64_t> current;
  vector<uint64_t> ordered_accesses;
  uint16_t capacity;
  uint64_t timestamp;
  bool has_built;
  uint64_t build_timestamp;

public:
  OPTgen(uint16_t c = 8) {
    capacity = c;
    timestamp = 0;
    has_built = false;
    build_timestamp = 0;
  }
  void incr_build_timestamp() { build_timestamp += 1; }
  void incr_timestamp() { timestamp += 1; }
  void reset_timestamp() { timestamp = 0; }
  uint64_t get_reuse_distance(uint64_t key, bool *last_access = nullptr) {
    auto it = future_accesses[key].upper_bound(timestamp);
    if (it == future_accesses[key].end()) {
      *last_access = true;
      return 0;
    }
    set<uint64_t> unique_cache_lines;
    for (uint64_t start = timestamp; start < *it; start++) {
      unique_cache_lines.insert(ordered_accesses[start]);
    }
    *last_access = false;
    uint64_t result = unique_cache_lines.size();
    unique_cache_lines.clear();
    return result;
  }
  void get_median_reuse_distance(unordered_map<uint64_t, uint64_t> &result) {
    result.clear();
    for (const auto &kv : future_accesses) {
      const auto key = kv.first;
      vector<uint64_t> reuse_distances;
      uint64_t last_access = 0;
      for (const auto &timestamp : kv.second) {
        if (last_access == 0) {
          //
        } else {
          set<uint64_t> unique_cache_lines;
          for (uint64_t start = last_access + 1; start < timestamp; start++) {
            unique_cache_lines.insert(ordered_accesses[start - 1]);
          }
          reuse_distances.push_back(unique_cache_lines.size());
        }
        last_access = timestamp;
      }
      auto s = reuse_distances.size();
      if (s < 1) {
        continue;
      }
      sort(reuse_distances.begin(), reuse_distances.end());
      uint64_t median;
      if (s % 2 == 0) {
        median = (reuse_distances[s / 2 - 1] + reuse_distances[s / 2]) / 2;
      } else {
        median = reuse_distances[s / 2];
      }
      result[key] = median;
    }
  }
  void add_future_access(uint64_t key) {
    assert(!has_built);
    incr_build_timestamp();
    if (!future_accesses.count(key)) {
      future_accesses[key] = set<uint64_t>();
    }
    future_accesses[key].insert(build_timestamp);
    ordered_accesses.push_back(key);
  }
  void add_future_prefetch(uint64_t key) {
    assert(!has_built);
    incr_build_timestamp();
    if (!future_prefetches.count(key)) {
      future_prefetches[key] = set<uint64_t>();
    }
    future_prefetches[key].insert(build_timestamp);
    ordered_accesses.push_back(key);
  }
  void finish_build() { has_built = true; }
  bool hit(uint64_t key) {
    assert(has_built);
    if (current.count(key))
      return true;
    else
      return false;
  }
  void insert(uint64_t key, bool is_prefetch = false,
              bool *has_evicted = nullptr, uint64_t *evicted = nullptr) {
    assert(has_built);
    if (current.size() == capacity) {
      auto current_it = future_accesses[key].upper_bound(timestamp);
      if (current_it == future_accesses[key].end()) {
        // there is no point caching the key
        if (has_evicted != nullptr && evicted != nullptr) {
          *has_evicted = true;
          *evicted = key;
        }
        return;
      }
      // first which one among the current set will be prefetched furthest
      bool prefetch_found = false;
      pair<uint64_t, uint64_t> candidate_based_on_prefetch;
      for (const auto &entry : current) {
        auto it = future_prefetches[entry].upper_bound(timestamp);
        if (it == future_prefetches[entry].end()) {
          // this won't be prefetched
        } else {
          // Now let's make sure that this prefetch is before the next load on
          // the same cache line
          auto demand_it = future_accesses[entry].upper_bound(timestamp);
          if (demand_it == future_accesses[entry].end()) {
            // No demand load so prefetch is the first one so no problem
          } else {
            // Demand load is present, so compare with prefetch time
            if ((*it) > (*demand_it)) {
              // prefetch comes after the first load so ignore this prefetch
              continue;
            }
          }
          if (prefetch_found) {
            if (candidate_based_on_prefetch.first < (*it)) {
              candidate_based_on_prefetch.first = *it;
              candidate_based_on_prefetch.second = entry;
            }
          } else {
            prefetch_found = true;
            candidate_based_on_prefetch.first = *it;
            candidate_based_on_prefetch.second = entry;
          }
        }
      }
      // if no such prefetch, we can go with the current optimal algorithm
      if (prefetch_found) {
        current.erase(candidate_based_on_prefetch.second);

        if (has_evicted != nullptr && evicted != nullptr) {
          *has_evicted = true;
          *evicted = candidate_based_on_prefetch.second;
        }
      } else {
        pair<uint64_t, uint64_t> candidate;
        auto it = future_accesses[key].upper_bound(timestamp);
        if (it == future_accesses[key].end()) {
          // there is no point caching the key
          if (has_evicted != nullptr && evicted != nullptr) {
            *has_evicted = true;
            *evicted = key;
          }
          return;
        }
        candidate.first = *it;
        candidate.second = key;
        for (const auto &entry : current) {
          auto it = future_accesses[entry].upper_bound(timestamp);
          if (it == future_accesses[entry].end()) {
            candidate.first = build_timestamp + 1;
            candidate.second = entry;
          } else if (candidate.first < (*it)) {
            candidate.first = *it;
            candidate.second = entry;
          }
        }
        if (candidate.second == key) {
          // bypass since it is not beneficial to cache the key

          if (has_evicted != nullptr && evicted != nullptr) {
            *has_evicted = true;
            *evicted = key;
          }
          return;
        } else {
          current.erase(candidate.second);

          if (has_evicted != nullptr && evicted != nullptr) {
            *has_evicted = true;
            *evicted = candidate.second;
          }
        }
      }
    } else {
      if (has_evicted != nullptr) {
        *has_evicted = false;
      }
    }
    current.insert(key);
  }
  bool access(uint64_t key, bool is_prefetch = false,
              bool *has_evicted = nullptr, uint64_t *evicted = nullptr) {
    assert(has_built);
    incr_timestamp();
    if (hit(key)) {
      if (has_evicted != nullptr) {
        *has_evicted = false;
      }
      return true;
    } else {
      insert(key, is_prefetch, has_evicted, evicted);
      return false;
    }
  }
};

class Hawkeye {
private:
  unordered_map<uint64_t, uint8_t> pc_counter;

public:
  unordered_map<uint64_t, pair<uint64_t, uint64_t>> pc_positive_negative_counts;
  Hawkeye() {
    //
  }
  void update(uint64_t pc, bool hit) {
    if (pc_counter.count(pc) == 0) {
      pc_counter[pc] = 4;
    }
    if (hit) {
      // increase if less than 7
      if (pc_counter[pc] < 7) {
        pc_counter[pc]++;
      }
    } else {
      // decrease if greater than 0
      if (pc_counter[pc] > 0) {
        pc_counter[pc]--;
      }
    }
  }
  bool predict(uint64_t pc) {
    bool result = false;
    if (pc_counter.count(pc) == 0 || pc_counter[pc] > 3) {
      result = true;
    }
    if (pc_positive_negative_counts.count(pc) == 0) {
      pc_positive_negative_counts[pc] = make_pair(0, 0);
    }
    if (result) {
      pc_positive_negative_counts[pc].second++;
    } else {
      pc_positive_negative_counts[pc].first++;
    }
    return result;
  }
};

// some arguments from their source code
// deadThresh is missing in their code, so I use it as same as bypassThresh
const int numCounts = 4096 * 1;
const int numPredTables = 3;
const static int counterSize = 4; // for n-bit counter, set it to 2^n
const int bypassThresh = 3, deadThresh = 3;
int predTables[numCounts][numPredTables];
void init_pred_tables() {
  for (int i = 0; i < numCounts; i++) {
    for (int j = 0; j < numPredTables; j++) {
      predTables[i][j] = 0;
    }
  }
}

class GHRP {
private:
  struct cache_line {
    bool plru_timestamp;
    uint64_t lru_timestamp;
    uint16_t signature;
    bool dead_prediction;
  };
  map<uint64_t, cache_line> cache;

  uint32_t numLines;
  uint64_t current_timestamp;

public:
  GHRP(uint32_t capacity = 8) {
    numLines = capacity;
    current_timestamp = 0;
  }

  bool access(uint64_t addr, uint64_t pc, uint16_t global_history) {
    auto sign = make_signature(pc, global_history);
    auto cntrs = getCounters(computeIndices(sign));

    current_timestamp++;
    // update_history(addr);

    auto it = cache.find(addr);
    bool is_hit = (it != cache.end());
    if (!is_hit) {
      bool need_bypass = majorityVote(cntrs, bypassThresh);
      // if bypass, return false without doing evition or update cache lines
      if (need_bypass)
        return false;

      // miss
      auto block = do_evition();
      updatePredTable(computeIndices(block.signature), true);
      block.plru_timestamp = true;
      block.lru_timestamp = current_timestamp;
      block.dead_prediction = majorityVote(cntrs, deadThresh);
      block.signature = sign;
      cache.emplace(addr, block);
    } else {
      // hit
      auto &block = it->second;
      updatePredTable(computeIndices(block.signature), false);
      block.plru_timestamp = true;
      block.lru_timestamp = current_timestamp;
      block.dead_prediction = majorityVote(cntrs, deadThresh);
      block.signature = sign;
    }

    if (cache.size() == numLines) {
      bool all_set = true;
      for (const auto &kv : cache) {
        if (!kv.second.plru_timestamp) {
          all_set = false;
          break;
        }
      }
      if (all_set) {
        for (const auto &kv : cache) {
          cache[kv.first].plru_timestamp = false;
        }
      }
    }
    cache[addr].plru_timestamp = true;

    return is_hit;
  }

private:
  cache_line do_evition() {
    if (cache.size() < numLines)
      return cache_line();

    for (auto p : cache)
      if (p.second.dead_prediction) {
        cache.erase(p.first);
        return p.second;
      }

    // otherwise, return LRU block
    uint64_t oldest_addr = cache.begin()->first;
    for (auto p : cache) {
      if (is_ghrp_plru) {
        if (p.second.plru_timestamp == false) {
          oldest_addr = p.first;
          break;
        }
      } else {
        if (p.second.lru_timestamp < cache[oldest_addr].lru_timestamp) {
          oldest_addr = p.first;
        }
      }
    }
    assert(cache.count(oldest_addr));
    auto ret = cache[oldest_addr];
    cache.erase(oldest_addr);
    return ret;
  }

  inline uint16_t make_signature(uint64_t pc, uint16_t his) {
    return (uint16_t)(pc ^ his);
  }

  inline vector<uint64_t> computeIndices(uint16_t signature) {
    vector<uint64_t> indices;
    for (int i = 0; i < numPredTables; i++)
      indices.push_back(hash(signature, i));
    return indices;
  }
  inline vector<int> getCounters(vector<uint64_t> indices) {
    vector<int> counters;
    for (int t = 0; t < numPredTables; t++)
      counters.push_back(predTables[indices[t]][t]);
    return counters;
  }

  inline bool majorityVote(vector<int> counters, int threshold) {
    int vote = 0;
    for (int i = 0; i < numPredTables; i++)
      if (counters[i] >= threshold)
        vote++;
    return (vote * 2 >= numPredTables);
  }
  inline void updatePredTable(vector<uint64_t> indices, bool isDead) {
    for (int t = 0; t < numPredTables; t++) {
      if (isDead) {
        if (predTables[indices[t]][t] < counterSize - 1) {
          predTables[indices[t]][t] += 1;
        }
      } else {
        if (predTables[indices[t]][t] > 0) {
          predTables[indices[t]][t] -= 1;
        }
      }
    }
  }

  // 3 hash functions from their source code, with my adjustment
  typedef uint64_t UINT64;
  inline UINT64 mix(UINT64 a, UINT64 b, UINT64 c) {
    a -= b;
    a -= c;
    a ^= (c >> 13);
    b -= c;
    b -= a;
    b ^= (a << 8);
    c -= a;
    c -= b;
    c ^= (b >> 13);
    a -= b;
    a -= c;
    a ^= (c >> 12);
    b -= c;
    b -= a;
    b ^= (a << 16);
    c -= a;
    c -= b;
    c ^= (b >> 5);
    a -= b;
    a -= c;
    a ^= (c >> 3);
    b -= c;
    b -= a;
    b ^= (a << 10);
    c -= a;
    c -= b;
    c ^= (b >> 15);
    return c;
  }
  inline UINT64 f1(UINT64 x) {
    UINT64 fone = mix(0xfeedface, 0xdeadb10c, x);
    return fone;
  }
  inline UINT64 f2(UINT64 x) {
    UINT64 ftwo = mix(0xc001d00d, 0xfade2b1c, x);
    return ftwo;
  }
  inline UINT64 fi(UINT64 x) {
    UINT64 ind = (f1(x)) + (f2(x));
    return ind;
  }
  inline uint64_t hash(uint16_t signature, int i) {
    if (i == 0)
      return f1(signature) & (numCounts - 1);
    else if (i == 1)
      return f2(signature) & (numCounts - 1);
    else if (i == 2)
      return fi(signature) & (numCounts - 1);
    else
      assert(false);
  }
};

class SRRIP {
private:
  map<uint64_t, int> rrpv;
  uint16_t capacity;

public:
  SRRIP(uint16_t c = 8) : capacity(c) {}
  bool access(uint64_t key) {
    auto it = rrpv.find(key);
    bool hit = (it != rrpv.end());
    if (hit) {
      it->second = 0;
      return true;
    } else {
      if (rrpv.size() < capacity) {
        rrpv.emplace(key, 2);
        return false;
      }

      while (true) {
        for (auto p : rrpv)
          if (p.second == 3) {
            rrpv.erase(p.first);
            rrpv.emplace(key, 2);
            return false;
          }
        for (auto &p : rrpv)
          p.second++;
      }
    }
  }
};

// class BRRIP {
// private:
//   map<uint64_t, int> rrpv;
//   uint16_t capacity;

//   static int brrip_counter;

// public:
//   BRRIP(uint16_t c = 8) : capacity(c) {}

//   bool access(uint64_t key){
//     auto it = rrpv.find(key);
//     bool hit = (it != rrpv.end());
//     if (hit) {
//       it->second = 0;
//       return true;
//     } else {
//       if (rrpv.size() < capacity) {
//         after_eviction(key);
//         return false;
//       }

//       while (true) {
//         for (auto p : rrpv)
//           if (p.second == 3) {
//             rrpv.erase(p.first);
//             after_eviction(key);
//             return false;
//           }
//         for (auto &p : rrpv)
//           p.second++;
//       }
//     }
//   }

// private:
//   void after_eviction(uint64_t key){
//     if(brrip_counter == 1)
//       rrpv.emplace(key, 2);
//     else
//       rrpv.emplace(key, 3);

//     if(brrip_counter < 32)
//       brrip_counter++;
//     else
//       brrip_counter = 0;
//   }
// };

// int BRRIP::brrip_counter = 0;

class DRRIP {
public:
  enum SetType { TYPE_SRRIP, TYPE_BRRIP, TYPE_FOLLOWER };

private:
  uint16_t capacity;
  SetType drrip_set_type;
  map<uint64_t, int> rrpv;

  static int brrip_counter;
  static int psel_counter;

public:
  DRRIP(SetType type, uint16_t c = 8) : capacity(c), drrip_set_type(type) {}

  bool access(uint64_t key) {
    auto it = rrpv.find(key);
    bool hit = (it != rrpv.end());
    if (hit) {
      it->second = 0;
      return true;
    } else {
      if (rrpv.size() < capacity) {
        after_eviction(key);
        return false;
      }

      while (true) {
        for (auto p : rrpv)
          if (p.second == 3) {
            rrpv.erase(p.first);
            after_eviction(key);
            return false;
          }
        for (auto &p : rrpv)
          p.second++;
      }
    }
  }

  static void DRRIP_init(DRRIP *drrips[], int nsets, uint16_t c = 8) {
    brrip_counter = 0;
    psel_counter = 0;

    for (int i = 0; i < nsets; i++)
      if ((i + 1) % (nsets / 32 - 1) == 0)
        drrips[i] = new DRRIP(DRRIP::TYPE_BRRIP, c);
      else if (i % (nsets / 32 + 1) == 0)
        drrips[i] = new DRRIP(DRRIP::TYPE_SRRIP, c);
      else
        drrips[i] = new DRRIP(DRRIP::TYPE_FOLLOWER, c);
  }

private:
  void after_eviction(uint64_t key) {
    switch (drrip_set_type) {
    case TYPE_SRRIP:
      rrpv.emplace(key, 2);
      if (psel_counter < 1023) // 1024 in Kalla's code
        psel_counter++;
      break;

    case TYPE_BRRIP:
      if (brrip_counter == 1)
        rrpv.emplace(key, 2);
      else
        rrpv.emplace(key, 3);
      if (psel_counter > 0)
        psel_counter--;
      if (brrip_counter < 32)
        brrip_counter++;
      else
        brrip_counter = 0;
      break;

    case TYPE_FOLLOWER:
      int psel_msb = psel_counter >> 9;
      if (psel_msb == 0) // following SRRIP
        rrpv.emplace(key, 2);
      else // following BRRIP
          if (brrip_counter == 1)
        rrpv.emplace(key, 2);
      else
        rrpv.emplace(key, 3);
    }
  }
};

int DRRIP::brrip_counter = 0;
int DRRIP::psel_counter = 0;

class PLRU {
private:
  uint16_t capacity;
  unordered_map<uint64_t, bool> current;

public:
  PLRU(uint16_t c = 8) {
    capacity = c;
    srand(0);
  }
  bool hit(uint64_t key) {
    if (current.count(key) == 0) {
      return false;
    } else {
      return true;
    }
  }
  bool exp_evict(uint64_t key) {
    if (current.count(key) != 0) {
      current.erase(key);
      return true;
    } else {
      return false;
    }
  }
  bool access(uint64_t key, bool *was_capacity_full = nullptr,
              uint64_t *evicted = nullptr) {
    bool result = hit(key);
    if (result) {
    } else {
      if (current.size() == capacity) {
        // evict
        if (was_capacity_full != nullptr) {
          *was_capacity_full = false;
        }
        if (is_plru_random) {
          // pick a random one
          uint16_t random_index = rand() % capacity;
          uint16_t i = 0;
          uint64_t to_be_evicted;
          bool found = false;
          for (const auto &kv : current) {
            if (i == random_index) {
              to_be_evicted = kv.first;
              found = true;
              break;
            }
            i += 1;
          }
          assert(found);
          if (evicted != nullptr) {
            *evicted = to_be_evicted;
          }
          current.erase(to_be_evicted);
        } else {
          // pick the first one with zero and evict it
          uint64_t to_be_evicted;
          bool found = false;
          for (const auto &kv : current) {
            if (!kv.second) {
              to_be_evicted = kv.first;
              found = true;
              break;
            }
          }
          assert(found);
          if (evicted != nullptr) {
            *evicted = to_be_evicted;
          }
          current.erase(to_be_evicted);
        }
      } else {
        // no evict
        if (was_capacity_full != nullptr) {
          *was_capacity_full = false;
        }
      }
    }
    current[key] = true;
    if (current.size() == capacity) {
      bool all_set = true;
      for (const auto &kv : current) {
        if (!kv.second) {
          all_set = false;
          break;
        }
      }
      if (all_set) {
        for (const auto &kv : current) {
          current[kv.first] = false;
        }
      }
    }
    current[key] = true;
    return result;
  }
};

class LRU {
private:
  uint16_t capacity;
  unordered_map<uint64_t, uint8_t> current_rrips;
  // vector<uint64_t> all_accesses;
  // vector<pair<uint64_t, uint64_t>> all_evictions;

public:
  LRU(uint16_t c = 8) { capacity = c; }
  bool hit(uint64_t key) {
    if (current_rrips.count(key) == 0)
      return false;
    else
      return true;
  }
  bool exp_evict(uint64_t key) {
    if (current_rrips.count(key) != 0) {
      current_rrips.erase(key);
      // all_evictions.push_back(make_pair(key, all_accesses.size() - 1));
      return true;
    } else {
      return false;
    }
  }
  bool access(uint64_t key, bool is_cache_friendly,
              bool *was_capacity_full = nullptr, uint64_t *evicted = nullptr) {
    bool result = hit(key);
    if (result) {
      if (is_cache_friendly) {
        current_rrips[key] = 0;
      } else {
        current_rrips[key] = 7;
      }
    } else {
      if (current_rrips.size() == capacity) {
        if (was_capacity_full != nullptr) {
          *was_capacity_full = true;
        }
        // need to evict
        uint64_t first_averse;
        bool averse_found = false;
        for (const auto &KV : current_rrips) {
          if (KV.second == 7) {
            averse_found = true;
            first_averse = KV.first;
            break;
          }
        }
        if (averse_found) {
          current_rrips.erase(first_averse);
          if (evicted != nullptr) {
            *evicted = first_averse;
          }
          if (is_cache_friendly) {
            for (auto &KV : current_rrips) {
              uint64_t first = KV.first;
              if (current_rrips[first] < 6) {
                current_rrips[first]++;
              }
            }
            current_rrips[key] = 0;
          } else {
            current_rrips[key] = 7;
          }
        } else {
          if (is_cache_friendly) {
            // remove the oldest non averse one
            uint64_t oldest_non_averse;
            uint64_t oldest_age;
            bool is_first = true;
            for (const auto &KV : current_rrips) {
              if (is_first) {
                oldest_age = KV.second;
                oldest_non_averse = KV.first;
                is_first = false;
              } else if (oldest_age < KV.second) {
                oldest_age = KV.second;
                oldest_non_averse = KV.first;
              }
            }
            current_rrips.erase(oldest_non_averse);
            if (evicted != nullptr) {
              *evicted = oldest_non_averse;
            }
            for (auto &KV : current_rrips) {
              uint64_t first = KV.first;
              if (current_rrips[first] < 6) {
                current_rrips[first]++;
              }
            }
            current_rrips[key] = 0;
          } else {
            // just remove this one, i.e., bypassing
          }
        }
      } else {
        // no need to evict
        if (was_capacity_full != nullptr) {
          *was_capacity_full = false;
        }
        if (is_cache_friendly) {
          for (auto &KV : current_rrips) {
            uint64_t first = KV.first;
            if (current_rrips[first] < 6) {
              current_rrips[first]++;
            }
          }
          current_rrips[key] = 0;
        } else {
          current_rrips[key] = 7;
        }
      }
    }
    return result;
  }
};

class HARMONY {
private:
  OPTgen l1i_opt[64];
  Hawkeye hawkeyes[64];
  LRU l1i[64];
  PLRU basic_lru[64];
  PLRU pgo_lru[64];
  GHRP ghrp[64];
  SRRIP srrip[64];
  DRRIP *drrip[64];

  unordered_map<string, vector<bool>> results;

  vector<uint64_t> pcs;
  vector<vector<uint64_t>> cl_address;

  vector<unordered_map<vector<uint64_t>, unordered_map<uint64_t, uint64_t>, container_hash>>
      opt_memorization_table;
  unordered_map<uint64_t, uint64_t> evict_counters;
  unordered_map<uint64_t, uint64_t> last_access_to_this_cl;

  uint64_t empty_not_empty_windows[2] = {0, 0};

  void init_opt_memorization_table() {
    opt_memorization_table.clear();
    for (uint8_t history_size = 1; history_size <= 64; history_size *= 2) {
      opt_memorization_table.push_back(
          unordered_map<vector<uint64_t>, unordered_map<uint64_t, uint64_t>, container_hash>());
    }
  }
  uint64_t hist_helper(vector<uint64_t> &history) {
    auto size = history.size();
    assert(size);
    uint64_t bitmask = 0xFFFFFFFFFFFFFFFF;
    uint8_t shift = 0;
    uint8_t lshift = 0;
    if (size == 1) {
      return history[0];
    }
    /*
    1->64->0xFFFFFFFFFFFFFFFF-> >>0
    2->32->0x00000000FFFFFFFF-> >>0
    3,4->16->0x000000000000FFFF-> >>0
    5-8->8->56->0x0000000000000FF0-> >>4
    9-16->4->60->0x00000000000003C0-> >>6
    17-32->2->62->0x0000000000000180-> >>7
    33-64->1->63->0x0000000000000040-> >>6
    */
    if (size == 2) {
      bitmask = 0x00000000FFFFFFFF;
      shift = 0;
      lshift = 32;
    } else if (size > 2 && size < 5) {
      bitmask = 0x000000000000FFFF;
      shift = 0;
      lshift = 16;
    } else if (size > 4 && size < 9) {
      bitmask = 0x0000000000000FF0;
      shift = 4;
      lshift = 8;
    } else if (size > 8 && size < 17) {
      bitmask = 0x00000000000003C0;
      shift = 6;
      lshift = 4;
    } else if (size > 16 && size < 33) {
      bitmask = 0x0000000000000180;
      shift = 7;
      lshift = 2;
    } else if (size > 32 && size < 65) {
      bitmask = 0x0000000000000040;
      shift = 6;
      lshift = 1;
    } else {
      return history[0];
    }
    uint64_t result = 0;
    for (int i = 0; i < size; i++) {
      result = (result << lshift) | ((history[i] & bitmask) >> shift);
    }
    return result;
  }
  void calculate_history(uint64_t k, vector<vector<uint64_t>> &histories) {
    assert(k >= 0);
    assert(k < pcs.size());
    histories.clear();
    /*
    1->whole 64-bit
    2-> 32,32
    4->16,16,16,16
    8->8,8,8,8,8,8,8,8
    16->4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
    32->2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
    64->1bit
    */
    for (uint8_t history_size = 1; history_size <= 64; history_size *= 2) {
      // uint64_t history = pcs[k];
      vector<uint64_t> tmp;
      tmp.push_back(pcs[k]);
      for (int64_t j = k - 1; j > k - history_size && j >= 0 && j < pcs.size();
           j--) {
        if (j < 0) {
          break;
        }
        // history ^= pcs[j];
        tmp.push_back(pcs[j]);
      }
      // histories.push_back(history);
      histories.push_back(tmp);
      //histories.push_back(hist_helper(tmp));
      //tmp.clear();
    }
  }
  void update_memorization_table(bool has_evicted, uint64_t evicted, uint64_t i,
                                 CFG &dynamic_cfg) {
    uint64_t evicted_bbl_pc = 0;
    if (has_evicted) {
      if (evicted != 0) {
        evict_counters[evicted] += 1;
        std::vector<std::set<vector<uint64_t>>> candidate_pcs;
        for (uint8_t history_size = 1; history_size <= 64; history_size *= 2) {
          candidate_pcs.push_back(std::set<vector<uint64_t>>());
        }
        if (last_access_to_this_cl.count(evicted) > 0) {
          uint64_t j = last_access_to_this_cl[evicted];
          evicted_bbl_pc = pcs[j];
          for (uint64_t k = j; k <= j + 10; k++) {
            if (k >= i - 10) {
              break;
            }
            if (dynamic_cfg.is_self_modifying(pcs[k])) {
              continue;
            }
            vector<vector<uint64_t>> current_history;
            calculate_history(k, current_history);
            for (int l = 0; l < current_history.size(); l++) {
              assert(l < candidate_pcs.size());
              candidate_pcs[l].insert(current_history[l]);
            }
          }
          if (i > 0) {
            for (uint64_t k = i - 1; k >= i - 10; k--) {
              if (k <= j + 10) {
                break;
              }
              if (dynamic_cfg.is_self_modifying(pcs[k])) {
                continue;
              }
              vector<vector<uint64_t>> current_history;
              calculate_history(k, current_history);
              for (int l = 0; l < current_history.size(); l++) {
                assert(l < candidate_pcs.size());
                candidate_pcs[l].insert(current_history[l]);
              }
            }
          }
        }
        bool is_empty = true;
        for (int l = 0; l < candidate_pcs.size(); l++) {
          const auto &current_candidate = candidate_pcs[l];
          if (current_candidate.size()) {
            is_empty = false;
          }
        }
        if (true) {
          vector<vector<uint64_t>> current_history;
          calculate_history(i, current_history);
          for (int l = 0; l < current_history.size(); l++) {
            assert(l < candidate_pcs.size());
            candidate_pcs[l].insert(current_history[l]);
          }
        }
        if (evicted_bbl_pc == 0 || ((evicted_bbl_pc >> 6) != evicted)) {
          evicted_bbl_pc = evicted << 6;
        }
        for (int l = 0; l < candidate_pcs.size(); l++) {
          const auto &current_candidate = candidate_pcs[l];
          assert(l < opt_memorization_table.size());
          for (const auto &candidate : current_candidate) {
            if (opt_memorization_table[l].count(candidate) == 0) {
              opt_memorization_table[l][candidate] =
                  unordered_map<uint64_t, uint64_t>();
            }
            if (opt_memorization_table[l][candidate].count(evicted) == 0) {
              opt_memorization_table[l][candidate][evicted] = 1;
            } else {
              opt_memorization_table[l][candidate][evicted] += 1;
            }
          }
        }
        if (is_empty) {
          empty_not_empty_windows[0] += 1;
        } else {
          empty_not_empty_windows[1] += 1;
        }
      }
    }
  }

public:
  HARMONY(CFG &dynamic_cfg, uint64_t best_probability, uint8_t p_setup) {
    prefetch_setup = p_setup;
    results["LRU"] = vector<bool>();
    results["GHRP"] = vector<bool>();
    results["SRRIP"] = vector<bool>();
    results["DRRIP"] = vector<bool>();
    results["Harmony"] = vector<bool>();
    results["OPT"] = vector<bool>();
    results["PGO"] = vector<bool>();
    init_pred_tables();
    init_opt_memorization_table();
    for (const auto &bbl_start_pc : dynamic_cfg.get_full_ordered_bbls()) {
      uint64_t start_cl_address = bbl_start_pc >> 6;
      uint64_t end_cl_address =
          (bbl_start_pc + dynamic_cfg.get_bbl_size(bbl_start_pc)) >> 6;
      cl_address.push_back(vector<uint64_t>());
      pcs.push_back(bbl_start_pc);
      for (uint64_t current = start_cl_address; current <= end_cl_address;
           current++) {
        cl_address[cl_address.size() - 1].push_back(current);
      }
    }
    for (uint64_t i = 0; i < cl_address.size(); i++) {
      auto &pc = pcs[i];
      auto &keys = cl_address[i];
      for (const auto &key : keys) {
        uint64_t set = key & 63;
        l1i_opt[set].add_future_access(key);
      }
      if ((prefetch_setup == 1 || prefetch_setup == 2) &&
          !dynamic_cfg.is_self_modifying(pc)) {
        uint64_t prefetch_target = keys[keys.size() - 1] + 1; // Next line
        if (prefetch_setup == 2) {                            // FDIP
          uint64_t fdip_target;
          bool result = dynamic_cfg.get_btb_target(pc, fdip_target);
          if (result) {
            prefetch_target = fdip_target;
          }
        }
        uint64_t set = prefetch_target & 63;
        l1i_opt[set].add_future_prefetch(prefetch_target);
      }
    }
    uint64_t hit_counts[64];
    uint64_t miss_counts[64];
    for (uint8_t i = 0; i < 64; i++) {
      hit_counts[i] = 0;
      miss_counts[i] = 0;
      l1i_opt[i].finish_build();
    }
    DRRIP::DRRIP_init(drrip, 64);

    uint64_t hit = 0;
    uint64_t miss = 0;
    uint64_t optimal_hit = 0;
    uint64_t optimal_miss = 0;
    uint64_t lru_hit = 0;
    uint64_t lru_miss = 0;
    uint64_t ghrp_hit = 0, ghrp_miss = 0;
    uint64_t srrip_hit = 0, srrip_miss = 0;
    uint64_t drrip_hit = 0, drrip_miss = 0;
    uint16_t global_history = 0;
    for (uint64_t i = 0; i < cl_address.size(); i++) {
      auto &pc = pcs[i];
      auto &keys = cl_address[i];
      for (auto &key : keys) {
        uint64_t set = key & 63;
        bool prediction = hawkeyes[0].predict(pc);
        if (l1i[set].access(key, prediction)) {
          hit++;
          hit_counts[set]++;
          results["Harmony"].push_back(true);
        } else {
          miss++;
          miss_counts[set]++;
          results["Harmony"].push_back(false);
        }
        if (basic_lru[set].access(key)) {
          lru_hit++;
          results["LRU"].push_back(true);
        } else {
          lru_miss++;
          results["LRU"].push_back(false);
        }
        if (ghrp[set].access(key, pc, global_history)) {
          ghrp_hit++;
          results["GHRP"].push_back(true);
        } else {
          ghrp_miss++;
          results["GHRP"].push_back(false);
        }
        if (srrip[set].access(key)) {
          srrip_hit++;
          results["SRRIP"].push_back(true);
        } else {
          srrip_miss++;
          results["SRRIP"].push_back(false);
        }
        if (drrip[set]->access(key)) {
          drrip_hit++;
          results["DRRIP"].push_back(true);
        } else {
          drrip_miss++;
          results["DRRIP"].push_back(false);
        }

        bool has_evicted = false;
        uint64_t evicted = 0;
        bool opt_result =
            l1i_opt[set].access(key, false, &has_evicted, &evicted);
        update_memorization_table(has_evicted, evicted, i, dynamic_cfg);
        hawkeyes[0].update(pc, opt_result);
        if (opt_result) {
          optimal_hit++;
          results["OPT"].push_back(true);
        } else {
          optimal_miss++;
          results["OPT"].push_back(false);
        }
        last_access_to_this_cl[key] = i;
      }
      if ((prefetch_setup == 1 || prefetch_setup == 2) &&
          !dynamic_cfg.is_self_modifying(pc)) {
        uint64_t prefetch_target = keys[keys.size() - 1] + 1; // Next line
        if (prefetch_setup == 2) {                            // FDIP
          uint64_t fdip_target;
          bool result = dynamic_cfg.get_btb_target(pc, fdip_target);
          if (result) {
            prefetch_target = fdip_target;
          }
        }
        uint64_t set = prefetch_target & 63;
        srrip[set].access(prefetch_target);
        drrip[set]->access(prefetch_target);
        basic_lru[set].access(prefetch_target);
        ghrp[set].access(prefetch_target, pc, global_history);
        bool prediction = hawkeyes[0].predict(pc);
        l1i[set].access(prefetch_target, prediction);
        bool has_evicted = false;
        uint64_t evicted = 0;
        bool opt_prefetch_result =
            l1i_opt[set].access(prefetch_target, true, &has_evicted, &evicted);
        update_memorization_table(has_evicted, evicted, i, dynamic_cfg);
        hawkeyes[0].update(pc, opt_prefetch_result);
        last_access_to_this_cl[prefetch_target] = i;
      }
      global_history = (global_history << 4) | ((pc & 7) << 1);
    }

    unordered_map<uint64_t, uint64_t> median_reuse_distances;
    cout << "LRU" << ' ';
    cout << ((lru_miss * 1.0) / 100000.0) << ' ';
    cout << lru_hit << ' ' << lru_miss << endl;
    cout << "GHRP" << ' ';
    cout << ((ghrp_miss * 1.0) / 100000.0) << ' ';
    cout << ghrp_hit << ' ' << ghrp_miss << endl;
    cout << "SRRIP" << ' ';
    cout << ((srrip_miss * 1.0) / 100000.0) << ' ';
    cout << srrip_hit << ' ' << srrip_miss << endl;
    cout << "DRRIP" << ' ';
    cout << ((drrip_miss * 1.0) / 100000.0) << ' ';
    cout << drrip_hit << ' ' << drrip_miss << endl;
    cout << "Hawkeye/Harmony" << ' ';
    cout << ((miss * 1.0) / 100000.0) << ' ';
    cout << hit << ' ' << miss << endl;
    cout << "OPT" << ' ';
    cout << ((optimal_miss * 1.0) / 100000.0) << ' ';
    cout << optimal_hit << ' ' << optimal_miss << endl;
    /*cout << empty_not_empty_windows[0] << ' ' << empty_not_empty_windows[1]
         << endl;*/
    if (no_design_space_explore) {
      return;
    }
    /*for (uint8_t i = 0; i < 64; i++) {
      cout << ((100.0 * miss_counts[i]) / (miss_counts[i] + hit_counts[i]))
           << ' ';
    }
    cout << endl;*/
    double bias = 0.0;
    double total = 0.0;
    uint64_t counts[2] = {0, 0};
    for (int i = 0; i < 64; i++) {
      auto &current = hawkeyes[i];
      for (const auto &KV : current.pc_positive_negative_counts) {
        bias += max(KV.second.first, KV.second.second);
        total += (KV.second.first + KV.second.second);
        counts[0] += KV.second.first;
        counts[1] += KV.second.second;
      }
      unordered_map<uint64_t, uint64_t> tmp;
      l1i_opt[i].get_median_reuse_distance(tmp);
      for (const auto &kv : tmp) {
        median_reuse_distances[kv.first] = kv.second;
      }
      tmp.clear();
    }
    OPTgen l1i_next[64];
    if (measure_coverage_accuracy) {
      for (uint64_t i = 0; i < cl_address.size(); i++) {
        auto &pc = pcs[i];
        auto &keys = cl_address[i];
        for (const auto &key : keys) {
          uint64_t set = key & 63;
          l1i_next[set].add_future_access(key);
        }
        if ((prefetch_setup == 1 || prefetch_setup == 2) &&
            !dynamic_cfg.is_self_modifying(pc)) {
          uint64_t prefetch_target = keys[keys.size() - 1] + 1; // Next line
          if (prefetch_setup == 2) {                            // FDIP
            uint64_t fdip_target;
            bool result = dynamic_cfg.get_btb_target(pc, fdip_target);
            if (result) {
              prefetch_target = fdip_target;
            }
          }
          uint64_t set = prefetch_target & 63;
          l1i_next[set].add_future_prefetch(prefetch_target);
        }
      }
      for (uint8_t i = 0; i < 64; i++) {
        l1i_next[i].finish_build();
      }
    }
    vector<unordered_map<vector<uint64_t>, uint64_t, container_hash>> bbl_counts;
    for (uint8_t l = 1; l <= 64; l *= 2) {
      bbl_counts.push_back(unordered_map<vector<uint64_t>, uint64_t, container_hash>());
    }
    for (uint64_t i = 0; i < pcs.size(); i++) {
      vector<vector<uint64_t>> current_history;
      calculate_history(i, current_history);
      for (int l = 0; l < current_history.size(); l++) {
        assert(l < bbl_counts.size());
        auto &bbl = current_history[l];
        if (!bbl_counts[l].count(bbl)) {
          bbl_counts[l][bbl] = 1;
        } else {
          bbl_counts[l][bbl] += 1;
        }
      }
    }
    /*cout << ((100.0 * bias) / total) << ' '
         << (100.0 * counts[0]) / (counts[0] + counts[1]) << endl;*/
    for (int start = 1; start <= 100; start += 1) {
      uint64_t pgo_hit = 0;
      uint64_t pgo_miss = 0;
      vector<unordered_map<vector<uint64_t>, std::set<uint64_t>, container_hash>> evict_maps;
      vector<unordered_map<uint64_t, uint64_t>> covered_evicts;
      for (uint8_t l = 1; l <= 64; l *= 2) {
        evict_maps.push_back(unordered_map<vector<uint64_t>, std::set<uint64_t>, container_hash>());
        covered_evicts.push_back(unordered_map<uint64_t, uint64_t>());
      }
      for (int table_index = 0; table_index < opt_memorization_table.size();
           table_index++) {
        for (const auto &it : opt_memorization_table[table_index]) {
          const auto &pc = it.first;
          assert(table_index < bbl_counts.size());
          assert(bbl_counts[table_index].count(pc));
          uint64_t bbl_count = bbl_counts[table_index][pc];
          std::set<uint64_t> evict_cls;
          for (const auto &evicts : it.second) {
            uint64_t target = evicts.first;
            uint64_t evict_count = evicts.second;
            double ratio = ((100.0 * evict_count) / bbl_count);
            if (ratio >= start) {
              evict_cls.insert(target);
              assert(table_index < covered_evicts.size());
              covered_evicts[table_index][target] += evict_count;
              /*if (median_reuse_distances.count(target) &&
                  median_reuse_distances[target] < 256) {
                //evict_cls.insert(target);
              }*/
            }
          }
          if (evict_cls.size() > 0) {
            assert(table_index < evict_maps.size());
            evict_maps[table_index][pc] = evict_cls;
          }
        }
      }
      //
      // uint64_t once_every_1000_insn = 10000;
      // int64_t last_exp_evict = -1;
      uint64_t lru_triggered = 0;
      uint64_t already_empty = 0;
      uint64_t reuse_higher_than_eight[2] = {0, 0};
      uint64_t reuse_lower_than_eight[2] = {0, 0};
      if (measure_coverage_accuracy) {
        for (uint8_t i = 0; i < 64; i++) {
          l1i_next[i].reset_timestamp();
        }
      }
      for (uint64_t i = 0; i < cl_address.size(); i++) {
        auto &pc = pcs[i];
        auto &keys = cl_address[i];

        vector<vector<uint64_t>> current_history;
        calculate_history(i, current_history);
        bool has_evicts = false;
        for (int l = 0; l < current_history.size(); l++) {
          assert(l < evict_maps.size());
          auto history = current_history[l];
          if (evict_maps[l].count(history) != 0) {
            has_evicts = true;
          }
        }

        for (const auto &key : keys) {
          uint64_t set = key & 63;
          if (measure_coverage_accuracy) {
            l1i_next[set].incr_timestamp();
          }
          bool this_key_has_evicts = false;
          for (int l = 0; l < current_history.size(); l++) {
            assert(l < evict_maps.size());
            auto history = current_history[l];
            if (evict_maps[l].count(history) != 0 &&
                evict_maps[l][history].count(key) != 0) {
              this_key_has_evicts = true;
            }
          }
          if (has_evicts && this_key_has_evicts) {
            // handle special case for self eviction
            if (pgo_lru[set].hit(key)) {
              pgo_hit++;
              results["PGO"].push_back(true);
            } else {
              pgo_miss++;
              results["PGO"].push_back(false);
            }
          } else {
            bool was_capacity_full;
            uint64_t evicted_cl;
            if (pgo_lru[set].access(key, &was_capacity_full, &evicted_cl)) {
              pgo_hit++;
              results["PGO"].push_back(true);
            } else {
              pgo_miss++;
              results["PGO"].push_back(false);
              if (measure_coverage_accuracy) {
                if (was_capacity_full) {
                  lru_triggered += 1;
                  bool has_no_access;
                  uint64_t reuse_distance = l1i_next[set].get_reuse_distance(
                      evicted_cl, &has_no_access);
                  if (has_no_access || reuse_distance >= 8) {
                    reuse_higher_than_eight[0] += 1;
                  } else {
                    reuse_lower_than_eight[0] += 1;
                  }
                } else {
                  already_empty += 1;
                }
              }
            }
          }
        }
        if ((prefetch_setup == 1 || prefetch_setup == 2) &&
            !dynamic_cfg.is_self_modifying(pc)) {
          uint64_t prefetch_target = keys[keys.size() - 1] + 1; // Next line
          if (prefetch_setup == 2) {                            // FDIP
            uint64_t fdip_target;
            bool result = dynamic_cfg.get_btb_target(pc, fdip_target);
            if (result) {
              prefetch_target = fdip_target;
            }
          }
          uint64_t set = prefetch_target & 63;
          // l1i_opt[set].add_future_prefetch(prefetch_target);

          bool this_key_has_evicts = false;
          for (int l = 0; l < current_history.size(); l++) {
            assert(l < evict_maps.size());
            auto history = current_history[l];
            if (evict_maps[l].count(history) != 0 &&
                evict_maps[l][history].count(prefetch_target) != 0) {
              this_key_has_evicts = true;
            }
          }

          if (has_evicts && this_key_has_evicts) {
            // avoid prefetching
          } else {
            pgo_lru[set].access(prefetch_target);
          }
        }
        if (has_evicts) { /* && (last_exp_evict == -1 ||
                            i - last_exp_evict >= once_every_1000_insn)) {*/
          for (int l = 0; l < current_history.size(); l++) {
            assert(l < evict_maps.size());
            auto &history = current_history[l];
            if (evict_maps[l].count(history) == 0) {
              continue;
            }
            for (const auto &cl : evict_maps[l][history]) {
              uint64_t set = cl & 63;
              bool was_present = pgo_lru[set].exp_evict(cl);
              if (measure_coverage_accuracy) {
                if (was_present) {
                  bool has_no_access;
                  uint64_t reuse_distance =
                      l1i_next[set].get_reuse_distance(cl, &has_no_access);
                  if (has_no_access || reuse_distance >= 8) {
                    reuse_higher_than_eight[1] += 1;
                  } else {
                    reuse_lower_than_eight[1] += 1;
                  }
                }
              }
            }
            // last_exp_evict = i;
          }
        }
      }
      cout << "PGO-LRU " << start << ' ';
      cout << ((pgo_miss * 1.0) / 100000.0) << ' ';
      if (measure_coverage_accuracy) {
        double coverage =
            (100.0 * already_empty) / (already_empty + lru_triggered);
        double pgo_accuracy =
            (100.0 * reuse_higher_than_eight[1]) /
            (reuse_higher_than_eight[1] + reuse_lower_than_eight[1]);
        double lru_accuracy =
            (100.0 * reuse_higher_than_eight[0]) /
            (reuse_higher_than_eight[0] + reuse_lower_than_eight[0]);
        double accuracy =
            (100.0 *
             (reuse_higher_than_eight[0] + reuse_higher_than_eight[1])) /
            (reuse_higher_than_eight[1] + reuse_lower_than_eight[1] +
             reuse_lower_than_eight[0] + reuse_higher_than_eight[0]);
        cout << pgo_hit << ' ' << pgo_miss << ' ' << coverage << ' ' << accuracy
             << ' ' << lru_accuracy << ' ' << pgo_accuracy << endl;
      } else {
        cout << pgo_hit << ' ' << pgo_miss << endl;
      }
    }
    /*for (const auto &KV : results) {
      const auto name = KV.first;
      ofstream stream_output;
      stream_output.open(name);
      for (const auto &hit_miss : KV.second) {
        stream_output << hit_miss << ' ';
      }
      stream_output.close();
    }*/
  }
};

#endif // HARMONY_H_
