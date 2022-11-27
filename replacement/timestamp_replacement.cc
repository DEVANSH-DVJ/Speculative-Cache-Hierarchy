#include "cache.h"

uint32_t CACHE::timestamp_victim(uint32_t cpu, uint64_t instr_id, uint32_t set,
                                 const BLOCK *current_set, uint64_t ip,
                                 uint64_t full_addr, uint32_t type) {
  uint32_t way = 0;

  // fill invalid line first
  for (way = 0; way < NUM_WAY_SPEC; way++) {
    if (spec_block[set][way].valid == false) {

      DP(if (warmup_complete[cpu]) {
        cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id
             << " invalid set: " << set << " way: " << way;
        cout << hex << " address: " << (full_addr >> LOG2_BLOCK_SIZE)
             << " victim address: " << spec_block[set][way].address
             << " data: " << spec_block[set][way].data;
        cout << dec << " lru: " << spec_block[set][way].lru << endl;
      });

      break;
    }
  }

  // Timestamp victim
  if (way == NUM_WAY_SPEC) {
    uint64_t max_timestamp = instr_id;
    uint32_t selected_way = NUM_WAY_SPEC;
    for (way = 0; way < NUM_WAY_SPEC; way++) {
      if (spec_block[set][way].instr_id > max_timestamp) {

        selected_way = way;
        max_timestamp = spec_block[set][way].instr_id;

        DP(if (warmup_complete[cpu]) {
          cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id
               << " replace set: " << set << " way: " << way;
          cout << hex << " address: " << (full_addr >> LOG2_BLOCK_SIZE)
               << " victim address: " << spec_block[set][way].address
               << " data: " << spec_block[set][way].data;
          cout << dec << " timestamp: " << spec_block[set][way].instr_id
               << endl;
        });

        break;
      }
    }
    way = selected_way;
    return way;
  }

  if (way == NUM_WAY_SPEC) {
    cerr << "[" << NAME << "] " << __func__ << " no victim! set: " << set
         << endl;
    assert(0);
  }

  return way;
}

void CACHE::timestamp_update(uint32_t set, uint32_t way) {}