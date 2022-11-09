#include "cache.h"
#include "ooo_cpu.h"
#include "set.h"

// Required for IPCP Prefetcher
#define PREF_CLASS_MASK                                                        \
  0xF00 // 0x3C000 //0x78000 //0x7800 //0x1e000 //0xF00	//according to IPCP
        // region size
#define NUM_OF_STRIDE_BITS                                                     \
  8 // 14 //15 //11 //13 //8			//according to IPCP region size

//---------------------------------------------- My #defines
//------------------------------------------------------
// ==> Declared in cache.h
//-----------------------------------------------------------------------------------------------------------------
//*==== Generic ====================================
uint64_t mshr_full_l0i = 0, mshr_full_l0d = 0, mshr_full_l1i = 0,
         mshr_full_l1d = 0, mshr_full_l2c = 0, mshr_full_llc = 0,
         prefetched_block_hit_mshr_buffer = 0;

//*==== IPCP ====================================
uint64_t prefetch_hits_l1d = 0, prefetch_hits_l2c = 0, prefetch_hits_l2c_RQ = 0,
         demand_misses = 0, load_accesses = 0, last_load_access = 0,
         l1d_hits = 0, l1d_miss = 0, l1d_access = 0, read_mshr_full = 0,
         prefetch_mshr_full = 0, l2c_handle_read = 0;

//*==== FNLMMA ==================================
uint64_t non_spec_fnlmma_call = 0, spec_fnlmma_call = 0;

uint8_t speculative_prefetch =
    1; // Beginning with speculative prefetching as ON

uint64_t l0d_accesses = 0, interval_l1d_misses = 0, useful_l1d = 0,
         useful_both = 0;

bool finalised = false;

float old_coverage = 0;

//*==============================================
// Count Variables
uint64_t l1d_call_count = 0, prev_instr_id = 0, curr_inst_id = 0, o3_count = 0,
         other_count = 0, l1d_map_size = 0, l1d_max_map_size = 0,
         handle_prefetch_l1d_call_count = 0, handle_prefetch_l2c_call_count = 0,
         IPCP_OFFSET_USEFUL_COUNT[NUM_OFFSETS],
         IPCP_OFFSET_USELESS_COUNT[NUM_OFFSETS],
         IPCP_OFFSET_ISSUED_COUNT[NUM_OFFSETS], l1i_map_size = 0,
         l1i_max_map_size = 0;

uint64_t l2pf_access = 0;

void CACHE::handle_fill() {
  // handle fill

  uint32_t fill_cpu = (MSHR.next_fill_index == MSHR_SIZE)
                          ? NUM_CPUS
                          : MSHR.entry[MSHR.next_fill_index].cpu;
  if (fill_cpu == NUM_CPUS)
    return;

  if (MSHR.next_fill_cycle <= current_core_cycle[fill_cpu]) {

#ifdef SANITY_CHECK
    if (MSHR.next_fill_index >= MSHR.SIZE)
      assert(0);
#endif

    uint32_t mshr_index = MSHR.next_fill_index;

    // Speculative Execution
    if (MSHR.entry[mshr_index].is_speculative == 1) {

      DPT(if (warmup_complete[fill_cpu]) {
        cout << "[" << NAME << "] " << __func__
             << " execution with speculative_bit = 1  current: "
             << current_core_cycle[fill_cpu];
        cout << endl;
      });

      if (cache_type == IS_L1I || cache_type == IS_L1D ||
          cache_type == IS_L2C || cache_type == IS_LLC) {

        // COLLECT STATS
        sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

        // Notifying the higher level MSHRs
        if (MSHR.entry[mshr_index].fill_level < fill_level) {

          if (fill_level == FILL_L2) {
            if (MSHR.entry[mshr_index].fill_l1i) {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
            if (MSHR.entry[mshr_index].fill_l1d) {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else if (fill_level == FILL_L1) {
            if (MSHR.entry[mshr_index].fill_l0i) {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }

            if (MSHR.entry[mshr_index].fill_l0d) {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else {
            // since multi-core can merge L2C misses from all cores
            if (MSHR.entry[mshr_index].instruction) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_icache[k]->return_data(&MSHR.entry[mshr_index]);
              }
            }

            if (MSHR.entry[mshr_index].is_data) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_dcache[k]->return_data(&MSHR.entry[mshr_index]);
              }
            }
          }
        }

        // Skipping filling to this cache level
        ;

        // Add miss latency
        if (warmup_complete[fill_cpu] &&
            (MSHR.entry[mshr_index].cycle_enqueued != 0)) {
          uint64_t current_miss_latency =
              (current_core_cycle[fill_cpu] -
               MSHR.entry[mshr_index].cycle_enqueued);
          total_miss_latency += current_miss_latency;

          DPT(if (warmup_complete[fill_cpu]) {
            cout << "[" << NAME << "] " << __func__
                 << "  miss latency for the mshr packet: "
                 << current_miss_latency << " instr_id: ";
            cout << MSHR.entry[mshr_index].instr_id << " address: " << hex
                 << MSHR.entry[mshr_index].address << dec << " full_addr: ";
            cout << hex << MSHR.entry[mshr_index].full_addr << dec
                 << " is_instruction: "
                 << uint32_t(MSHR.entry[mshr_index].instruction);
            cout << " is_data: " << MSHR.entry[mshr_index].data;
            cout << " MSHR cycle enqueued: "
                 << MSHR.entry[mshr_index].cycle_enqueued;
            cout << " event_cycle: " << MSHR.entry[mshr_index].event_cycle
                 << "  current: " << current_core_cycle[fill_cpu] << endl;
          });
        }

        DPT(if (warmup_complete[fill_cpu]) {
          cout << "[" << NAME << "] " << __func__
               << " removing from mshr without filling in cache; current: "
               << current_core_cycle[fill_cpu] << endl;
        });

        // Removing from MSHR
        MSHR.remove_queue(&MSHR.entry[mshr_index]);
        MSHR.num_returned--;

        update_fill_cycle();
      }

      else if (cache_type == IS_L0D || cache_type == IS_L0I) {

        // find victim
        uint32_t set = get_set(MSHR.entry[mshr_index].address), way;
        way = find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set,
                          block[set], MSHR.entry[mshr_index].ip,
                          MSHR.entry[mshr_index].full_addr,
                          MSHR.entry[mshr_index].type);

        uint8_t do_fill = 1;

        if (block[set][way].dirty) {

          // check if the lower level WQ has enough room to keep this writeback
          // request
          if (lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) ==
                lower_level->get_size(2, block[set][way].address)) {

              // lower level WQ is full, cannot replace this victim
              do_fill = 0;
              lower_level->increment_WQ_FULL(block[set][way].address);
              STALL[MSHR.entry[mshr_index].type]++;

              DPT(if (warmup_complete[fill_cpu]) {
                cout << "[" << NAME << "] " << __func__
                     << "do_fill: " << +do_fill;
                cout << " lower level wq is full!"
                     << " fill_addr: " << hex << MSHR.entry[mshr_index].address;
                cout << " victim_addr: " << block[set][way].tag << dec << endl;
              });
            } else {
              PACKET writeback_packet;

              writeback_packet.is_speculative = 0;
              writeback_packet.fill_level = fill_level << 1;
              writeback_packet.cpu = fill_cpu;
              writeback_packet.address = block[set][way].address;
              writeback_packet.full_addr = block[set][way].full_addr;
              writeback_packet.data = block[set][way].data;
              writeback_packet.instr_id = MSHR.entry[mshr_index].instr_id;
              writeback_packet.ip = 0; // writeback does not have ip
              writeback_packet.type = WRITEBACK;
              writeback_packet.event_cycle = current_core_cycle[fill_cpu];

              lower_level->add_wq(&writeback_packet);
            }
          }
        }

        // dirty block added to WQ of lower level
        if (do_fill) {

          // update replcement state for L0D and L0I
          update_replacement_state(
              fill_cpu, set, way, MSHR.entry[mshr_index].full_addr,
              MSHR.entry[mshr_index].ip, block[set][way].full_addr,
              MSHR.entry[mshr_index].type, 0);

          // COLLECT STATS
          sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
          sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

          DPT(if (warmup_complete[fill_cpu]) {
            cout << "[" << NAME << "] " << __func__
                 << " filling the cache when speculative_bit = 1  current: "
                 << current_core_cycle[fill_cpu] << endl;
          });

          fill_cache(set, way, &MSHR.entry[mshr_index]);

          // RFO marks cache line dirty
          if (cache_type == IS_L0D) {
            if (MSHR.entry[mshr_index].type == RFO)
              block[set][way].dirty = 1;
          }

          // No need to check the higher fill levels -> this is the highest
          // level
          ;

          DPT(if (warmup_complete[fill_cpu]) {
            if (cache_type == IS_L0I || cache_type == IS_L0D) {
              cout << "[" << NAME << "] " << __func__
                   << " adding to PROCESSED queue if not of PREFETCH type;  "
                      "current: "
                   << current_core_cycle[fill_cpu] << endl;
            }
          });

          // Update PROCESSED QUEUE
          if ((cache_type == IS_L0I) &&
              (MSHR.entry[mshr_index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&MSHR.entry[mshr_index]);
          } else if ((cache_type == IS_L0D) &&
                     (MSHR.entry[mshr_index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&MSHR.entry[mshr_index]);
          }

          // Update miss latency
          if (warmup_complete[fill_cpu] &&
              (MSHR.entry[mshr_index].cycle_enqueued != 0)) {
            uint64_t current_miss_latency =
                (current_core_cycle[fill_cpu] -
                 MSHR.entry[mshr_index].cycle_enqueued);
            total_miss_latency += current_miss_latency;

            DPT(if (warmup_complete[fill_cpu]) {
              cout << "[" << NAME << "] " << __func__
                   << "  miss latency for the mshr packet: "
                   << current_miss_latency << " instr_id: ";
              cout << MSHR.entry[mshr_index].instr_id << " address: " << hex
                   << MSHR.entry[mshr_index].address << dec << " full_addr: ";
              cout << hex << MSHR.entry[mshr_index].full_addr << dec
                   << " is_instruction: "
                   << uint32_t(MSHR.entry[mshr_index].instruction);
              cout << " is_data " << MSHR.entry[mshr_index].data;
              cout << " MSHR cycle enqueued: "
                   << MSHR.entry[mshr_index].cycle_enqueued;
              cout << " event_cycle: " << MSHR.entry[mshr_index].event_cycle
                   << "  current: " << current_core_cycle[fill_cpu] << endl;
            });
          }

          DPT(if (warmup_complete[fill_cpu]) {
            cout << "[" << NAME << "] " << __func__
                 << " removing from mshr after filling in cache; current: "
                 << current_core_cycle[fill_cpu] << endl;
          });

          // Remove from MSHR
          MSHR.remove_queue(&MSHR.entry[mshr_index]);
          MSHR.num_returned--;

          update_fill_cycle();
        }
      }

      else {
        // case for TLBs
        // We are not modeling L0TLB thus TLB packets won't have
        // (speculative_bit == 1) so this condition will not arrive.
        assert(0);
      }
    }

    // Non-speculative execution.
    else if (MSHR.entry[mshr_index].is_speculative == 0) {

      DPT(if (warmup_complete[fill_cpu]) {
        cout << "[" << NAME << "] " << __func__
             << " execution with speculative_bit = 0  current: "
             << current_core_cycle[fill_cpu];
        cout << endl;
      });

      // find victim
      uint32_t set = get_set(MSHR.entry[mshr_index].address), way;

      if (cache_type == IS_LLC) {
        way = llc_find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set,
                              block[set], MSHR.entry[mshr_index].ip,
                              MSHR.entry[mshr_index].full_addr,
                              MSHR.entry[mshr_index].type);
      } else
        way = find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set,
                          block[set], MSHR.entry[mshr_index].ip,
                          MSHR.entry[mshr_index].full_addr,
                          MSHR.entry[mshr_index].type);

#ifdef LLC_BYPASS
      if ((cache_type == IS_LLC) &&
          (way == LLC_WAY)) { // this is a bypass that does not fill the LLC

        // update replacement policy
        if (cache_type == IS_LLC) {
          llc_update_replacement_state(
              fill_cpu, set, way, MSHR.entry[mshr_index].full_addr,
              MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0);
        } else
          update_replacement_state(
              fill_cpu, set, way, MSHR.entry[mshr_index].full_addr,
              MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0);

        // COLLECT STATS
        if (cache_type == IS_L1D && fill_cpu == 0 &&
            MSHR.entry[mshr_index].type == 0) { //* @Tarun
          demand_misses++;
          // cout << "Cycle: " << current_core_cycle[0] << endl;
          // cout << "IP: 0x" << hex << MSHR.entry[mshr_index].ip << dec <<
          // endl; cout << "addr: 0x" << hex << MSHR.entry[mshr_index].full_addr
          // << dec << endl; cout << "(LLC BYPASS) Miss for block: 0x" << hex <<
          // MSHR.entry[mshr_index].address << dec << endl;
        }

        sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

        // cout << "checking fill level: start" << endl;
        // check fill level
        if (MSHR.entry[mshr_index].fill_level < fill_level) {

          if (fill_level == FILL_L2) {
            if (MSHR.entry[mshr_index].fill_l1i) {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
            if (MSHR.entry[mshr_index].fill_l1d) {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else if (fill_level == FILL_L1) {
            if (MSHR.entry[mshr_index].fill_l0i) //* L0I_CACHE
            {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
            if (MSHR.entry[mshr_index].fill_l0d) //* L0D_CACHE
            {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else {
            if (MSHR.entry[mshr_index].instruction)
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);

            if (MSHR.entry[mshr_index].is_data)
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
          }
        }

        // cout << "checking fill level: end" << endl;

        if (warmup_complete[fill_cpu] &&
            (MSHR.entry[mshr_index].cycle_enqueued != 0)) {
          uint64_t current_miss_latency =
              (current_core_cycle[fill_cpu] -
               MSHR.entry[mshr_index].cycle_enqueued);
          total_miss_latency += current_miss_latency;
        }

        MSHR.remove_queue(&MSHR.entry[mshr_index]);
        MSHR.num_returned--;

        update_fill_cycle();

        return; // return here, no need to process further in this function
      }
#endif

      uint8_t do_fill = 1;

      // is this dirty?
      if (block[set][way].dirty) {

        // check if the lower level WQ has enough room to keep this writeback
        // request
        if (lower_level) {
          if (lower_level->get_occupancy(2, block[set][way].address) ==
              lower_level->get_size(2, block[set][way].address)) {

            // lower level WQ is full, cannot replace this victim
            do_fill = 0;
            lower_level->increment_WQ_FULL(block[set][way].address);
            STALL[MSHR.entry[mshr_index].type]++;

            DP(if (warmup_complete[fill_cpu]) {
              cout << "[" << NAME << "] " << __func__
                   << "do_fill: " << +do_fill;
              cout << " lower level wq is full!"
                   << " fill_addr: " << hex << MSHR.entry[mshr_index].address;
              cout << " victim_addr: " << block[set][way].tag << dec << endl;
            });
          } else {
            PACKET writeback_packet;

            writeback_packet.is_speculative = 0;
            writeback_packet.fill_level = fill_level << 1;
            writeback_packet.cpu = fill_cpu;
            writeback_packet.address = block[set][way].address;
            writeback_packet.full_addr = block[set][way].full_addr;
            writeback_packet.data = block[set][way].data;
            writeback_packet.instr_id = MSHR.entry[mshr_index].instr_id;
            writeback_packet.ip = 0; // writeback does not have ip
            writeback_packet.type = WRITEBACK;
            writeback_packet.event_cycle = current_core_cycle[fill_cpu];

            lower_level->add_wq(&writeback_packet);
          }
        }
#ifdef SANITY_CHECK
        else {
          // sanity check
          if (cache_type != IS_STLB)
            assert(0);
        }
#endif
      }

      // victim is successfully removed from the cache and now we can bring new
      // cache line
      if (do_fill) {

        DPT(if (warmup_complete[fill_cpu]) {
          cout << "[" << NAME << "] " << __func__
               << " call to prefetcher_cache_fill;  current: "
               << current_core_cycle[fill_cpu];
          cout << endl;
        });

        // update prefetchers
#ifdef SPEC_COMMIT_L1I
        // FNL+MMA works on both L0 and L1 with SpecPref
        if (cache_type == IS_L0I)
          l1i_prefetcher_cache_fill(
              fill_cpu,
              ((MSHR.entry[mshr_index].ip) >> LOG2_BLOCK_SIZE)
                  << LOG2_BLOCK_SIZE,
              set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0,
              ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);

        if (cache_type == IS_L1I)
          l1i_prefetcher_cache_fill(
              fill_cpu,
              ((MSHR.entry[mshr_index].ip) >> LOG2_BLOCK_SIZE)
                  << LOG2_BLOCK_SIZE,
              set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0,
              ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);
#endif
#ifndef SPEC_COMMIT_L1I
        // blocks are only filled in the L1I cache
        if (cache_type == IS_L1I)
          l1i_prefetcher_cache_fill(
              fill_cpu,
              ((MSHR.entry[mshr_index].ip) >> LOG2_BLOCK_SIZE)
                  << LOG2_BLOCK_SIZE,
              set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0,
              ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);
#endif

#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
        if (cache_type == IS_L1D)
          l1d_prefetcher_cache_fill(
              MSHR.entry[mshr_index].full_addr, set, way,
              (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0,
              block[set][way].address << LOG2_BLOCK_SIZE,
              MSHR.entry[mshr_index].pf_metadata);
#endif

        if (cache_type == IS_L2C)
          MSHR.entry[mshr_index].pf_metadata = l2c_prefetcher_cache_fill(
              MSHR.entry[mshr_index].address << LOG2_BLOCK_SIZE, set, way,
              (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0,
              block[set][way].address << LOG2_BLOCK_SIZE,
              MSHR.entry[mshr_index].pf_metadata);

        // update replacement policy - since committed execution
        if (cache_type == IS_LLC) {
          llc_update_replacement_state(
              fill_cpu, set, way, MSHR.entry[mshr_index].full_addr,
              MSHR.entry[mshr_index].ip, block[set][way].full_addr,
              MSHR.entry[mshr_index].type, 0);
        } else
          update_replacement_state(
              fill_cpu, set, way, MSHR.entry[mshr_index].full_addr,
              MSHR.entry[mshr_index].ip, block[set][way].full_addr,
              MSHR.entry[mshr_index].type, 0);

        // COLLECT STATS
        sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

#ifdef IPCP_PREFETCHER
        // for IPCP Throttling.
        if (cache_type == IS_L1D || cache_type == IS_L0D) {
          if (MSHR.entry[mshr_index].late_pref == 1) {
            int temp_pf_class =
                (MSHR.entry[mshr_index].pf_metadata & PREF_CLASS_MASK) >>
                NUM_OF_STRIDE_BITS;
            if (temp_pf_class < 5) {
              pref_late[cpu][(
                  (MSHR.entry[mshr_index].pf_metadata & PREF_CLASS_MASK) >>
                  NUM_OF_STRIDE_BITS)]++;
            }
          }
        }
#endif

        DPT(if (warmup_complete[fill_cpu]) {
          cout << "[" << NAME << "] " << __func__
               << "  filling cache for speculative_bit: 0 "
               << " instr_id: ";
          cout << MSHR.entry[mshr_index].instr_id << " address: " << hex
               << MSHR.entry[mshr_index].address << dec << " full_addr: ";
          cout << hex << MSHR.entry[mshr_index].full_addr << dec
               << " is_instruction: "
               << uint32_t(MSHR.entry[mshr_index].instruction);
          cout << " is_data " << MSHR.entry[mshr_index].data;
          cout << " MSHR cycle enqueued: "
               << MSHR.entry[mshr_index].cycle_enqueued;
          cout << " event_cycle: " << MSHR.entry[mshr_index].event_cycle
               << "  current: " << current_core_cycle[fill_cpu] << endl;
        });

        // fill the block in the cache
        fill_cache(set, way, &MSHR.entry[mshr_index]);

        // RFO marks cache line dirty
        if (cache_type == IS_L0D) {
          if (MSHR.entry[mshr_index].type == RFO)
            block[set][way].dirty = 1;
        }

        // check fill level
        if (MSHR.entry[mshr_index].fill_level < fill_level) {

          if (fill_level == FILL_L2) {
            if (MSHR.entry[mshr_index].fill_l1i) {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
            if (MSHR.entry[mshr_index].fill_l1d) {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else if (fill_level == FILL_L1) {
            if (MSHR.entry[mshr_index].fill_l0i) {
              upper_level_icache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }

            if (MSHR.entry[mshr_index].fill_l0d) {
              upper_level_dcache[fill_cpu]->return_data(
                  &MSHR.entry[mshr_index]);
            }
          } else {
            if (MSHR.entry[mshr_index].instruction) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_icache[k]->return_data(&MSHR.entry[mshr_index]);
              }
            }

            if (MSHR.entry[mshr_index].is_data) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_dcache[k]->return_data(&MSHR.entry[mshr_index]);
              }
            }
          }
        }

        DPT(if (warmup_complete[fill_cpu]) {
          if (cache_type == IS_ITLB || cache_type == IS_DTLB ||
              cache_type == IS_L0I || cache_type == IS_L0D) {
            cout << "[" << NAME << "] " << __func__
                 << " adding to PROCESSED queue if not of PREFETCH type;  "
                    "current: "
                 << current_core_cycle[fill_cpu] << endl;
          }
        });

        // update processed packets
        if (cache_type == IS_ITLB) {
          MSHR.entry[mshr_index].instruction_pa = block[set][way].data;
          if (PROCESSED.occupancy < PROCESSED.SIZE)
            PROCESSED.add_queue(&MSHR.entry[mshr_index]);
        } else if (cache_type == IS_DTLB) {
          MSHR.entry[mshr_index].data_pa = block[set][way].data;
          if (PROCESSED.occupancy < PROCESSED.SIZE)
            PROCESSED.add_queue(&MSHR.entry[mshr_index]);
        }

        else if ((cache_type == IS_L0I) &&
                 (MSHR.entry[mshr_index].type != PREFETCH)) {
          if (PROCESSED.occupancy < PROCESSED.SIZE)
            PROCESSED.add_queue(&MSHR.entry[mshr_index]);
        } else if ((cache_type == IS_L0D) &&
                   (MSHR.entry[mshr_index].type != PREFETCH)) {
          if (PROCESSED.occupancy < PROCESSED.SIZE)
            PROCESSED.add_queue(&MSHR.entry[mshr_index]);
        }

        // Evaluate the miss latency
        if (warmup_complete[fill_cpu] &&
            (MSHR.entry[mshr_index].cycle_enqueued != 0)) {
          uint64_t current_miss_latency =
              (current_core_cycle[fill_cpu] -
               MSHR.entry[mshr_index].cycle_enqueued);
          total_miss_latency += current_miss_latency;

          DPT(if (warmup_complete[fill_cpu]) {
            cout << "[" << NAME << "] " << __func__
                 << "  miss latency for the mshr packet: "
                 << current_miss_latency << " instr_id: ";
            cout << MSHR.entry[mshr_index].instr_id << " address: " << hex
                 << MSHR.entry[mshr_index].address << dec << " full_addr: ";
            cout << hex << MSHR.entry[mshr_index].full_addr << dec
                 << " is_instruction: "
                 << uint32_t(MSHR.entry[mshr_index].instruction);
            cout << " is_data " << MSHR.entry[mshr_index].data;
            cout << " MSHR cycle enqueued: "
                 << MSHR.entry[mshr_index].cycle_enqueued;
            cout << " event_cycle: " << MSHR.entry[mshr_index].event_cycle
                 << "  current: " << current_core_cycle[fill_cpu] << endl;
          });
        }

        DPT(if (warmup_complete[fill_cpu]) {
          cout << "[" << NAME << "] " << __func__
               << " removing from mshr after filling in cache; current: "
               << current_core_cycle[fill_cpu] << endl;
        });

        MSHR.remove_queue(&MSHR.entry[mshr_index]);
        MSHR.num_returned--;

        update_fill_cycle();
      }
    }
  }
}

void CACHE::handle_writeback() {
  // handle write
  uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;
  if (writeback_cpu == NUM_CPUS)
    return;

  // handle the oldest entry
  if ((WQ.entry[WQ.head].event_cycle <= current_core_cycle[writeback_cpu]) &&
      (WQ.occupancy > 0)) {
    int index = WQ.head;

    if (WQ.entry[WQ.head].is_speculative == 1) {
      // WQ cannot have a speculative packet.
      assert(0);
    }

    // access cache
    uint32_t set = get_set(WQ.entry[index].address);
    int way = check_hit(&WQ.entry[index]);

    if (way >= 0) { // writeback hit (or RFO hit for L0D)

      // replacement state update allowed here because writeback blocks are
      // always committed.
      if (cache_type == IS_LLC) {
        llc_update_replacement_state(
            writeback_cpu, set, way, block[set][way].full_addr,
            WQ.entry[index].ip, 0, WQ.entry[index].type, 1);
      } else
        update_replacement_state(writeback_cpu, set, way,
                                 block[set][way].full_addr, WQ.entry[index].ip,
                                 0, WQ.entry[index].type, 1);

      // COLLECT STATS
      sim_hit[writeback_cpu][WQ.entry[index].type]++;
      sim_access[writeback_cpu][WQ.entry[index].type]++;

      // mark dirty
      block[set][way].dirty = 1;

      if (WQ.entry[index].prefetch == 1) {
        block[set][way].prefetch = 1;
      }

      if (cache_type == IS_ITLB)
        WQ.entry[index].instruction_pa = block[set][way].data;
      else if (cache_type == IS_DTLB)
        WQ.entry[index].data_pa = block[set][way].data;
      else if (cache_type == IS_STLB)
        WQ.entry[index].data = block[set][way].data;

      // check fill level
      if (WQ.entry[index].fill_level < fill_level) {

        if (fill_level == FILL_L2) {
          if (WQ.entry[index].fill_l1i) {
            upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);
          }
          if (WQ.entry[index].fill_l1d) {
            upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
          }
        } else if (fill_level == FILL_L1) //* L0D_CACHE   L0I_CACHE
        {
          if (WQ.entry[index].fill_l0i) {
            upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);
          }
          if (WQ.entry[index].fill_l0d) {
            upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
          }
        } else {
          if (WQ.entry[index].instruction)
            upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);

          if (WQ.entry[index].is_data)
            upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
        }
      }

      HIT[WQ.entry[index].type]++;
      ACCESS[WQ.entry[index].type]++;

      // remove this entry from WQ
      WQ.remove_queue(&WQ.entry[index]);
    }

    else { // writeback miss (or RFO miss for L0D)

      DP(if (warmup_complete[writeback_cpu]) {
        cout << "[" << NAME << "] " << __func__
             << " type: " << +WQ.entry[index].type << " miss";
        cout << " instr_id: " << WQ.entry[index].instr_id << " address: " << hex
             << WQ.entry[index].address;
        cout << " full_addr: " << WQ.entry[index].full_addr << dec;
        cout << " cycle: " << WQ.entry[index].event_cycle << endl;
      });

      if (cache_type == IS_L0D) { // RFO miss

        if (WQ.entry[index].type == COMMIT_WRITE) {
          // COMMIT_WRITE requests cannot arrive in L0D WQ
          assert(0);
        }

        // RFO concept:
        // RFO is a store request from the core. So to complete this, the
        // particular block has to be brought from the memory hierarchy to the
        // highest level cache. Thus a read request is added to the lower level
        // cache to fetch the block. When the block arrives in L0D through MSHR,
        // it is marked as dirty. The writeback request of other caches just
        // replaces the block that the WQ holds so it does not need the fetching
        // of any block from the lower level.

        // check mshr
        uint8_t miss_handled = 1;
        int mshr_index = check_mshr(&WQ.entry[index]);

        if (mshr_index == -2) {
          // this is a data/instruction collision in the MSHR, so we have to
          // wait before we can allocate this miss
          miss_handled = 0;
        }

        else if ((mshr_index == -1) &&
                 (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

          // add it to mshr (RFO miss)
          add_mshr(&WQ.entry[index]);

          // add it to the next level's read queue
          lower_level->add_rq(&WQ.entry[index]);
        } else {
          if ((mshr_index == -1) &&
              (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource

            // cannot handle miss request until one of MSHRs is available
            miss_handled = 0;
            STALL[WQ.entry[index].type]++;
          }

          else if (mshr_index != -1) { // already in-flight miss

            // update fill_level
            if (WQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
              MSHR.entry[mshr_index].fill_level = WQ.entry[index].fill_level;

            if ((WQ.entry[index].fill_l1i) &&
                (MSHR.entry[mshr_index].fill_l1i != 1)) {
              MSHR.entry[mshr_index].fill_l1i = 1;
            }
            if ((WQ.entry[index].fill_l1d) &&
                (MSHR.entry[mshr_index].fill_l1d != 1)) {
              MSHR.entry[mshr_index].fill_l1d = 1;
            }
            if ((WQ.entry[index].fill_l0i) &&
                (MSHR.entry[mshr_index].fill_l0i != 1)) {
              MSHR.entry[mshr_index].fill_l0i = 1;
            }
            if ((WQ.entry[index].fill_l0d) &&
                (MSHR.entry[mshr_index].fill_l0d != 1)) {
              MSHR.entry[mshr_index].fill_l0d = 1;
            }

            // update request
            if (MSHR.entry[mshr_index].type == PREFETCH) {

              uint8_t prior_returned = MSHR.entry[mshr_index].returned;
              uint64_t prior_event_cycle = MSHR.entry[mshr_index].event_cycle;

              uint8_t speculative_bit = WQ.entry[index].is_speculative &
                                        MSHR.entry[mshr_index].is_speculative;
              int fill_level = 0;

              uint8_t fill_l1d = 0, fill_l1i = 0, fill_l0d = 0, fill_l0i = 0;

              if (MSHR.entry[mshr_index].fill_level <
                  WQ.entry[index].fill_level) {
                fill_level = MSHR.entry[mshr_index].fill_level;
              } else {
                fill_level = WQ.entry[index].fill_level;
              }
              if (MSHR.entry[mshr_index].fill_l1d || WQ.entry[index].fill_l1d) {
                fill_l1d = 1;
              }
              if (MSHR.entry[mshr_index].fill_l1i || WQ.entry[index].fill_l1i) {
                fill_l1i = 1;
              }
              if (MSHR.entry[mshr_index].fill_l0d || WQ.entry[index].fill_l0d) {
                fill_l0d = 1;
              }
              if (MSHR.entry[mshr_index].fill_l0i || WQ.entry[index].fill_l0i) {
                fill_l0i = 1;
              }

              MSHR.entry[mshr_index] = WQ.entry[index];

              // in case request is already returned, we should keep event_cycle
              // and retunred variables
              MSHR.entry[mshr_index].returned = prior_returned;
              MSHR.entry[mshr_index].event_cycle = prior_event_cycle;

              MSHR.entry[mshr_index].is_speculative = speculative_bit;
              MSHR.entry[mshr_index].fill_level = fill_level;
              MSHR.entry[mshr_index].fill_l1d = fill_l1d;
              MSHR.entry[mshr_index].fill_l1i = fill_l1i;
              MSHR.entry[mshr_index].fill_l0d = fill_l0d;
              MSHR.entry[mshr_index].fill_l0i = fill_l0i;
            }

            MSHR_MERGED[WQ.entry[index].type]++;

            DP(if (warmup_complete[writeback_cpu]) {
              cout << "[" << NAME << "] " << __func__ << " mshr merged";
              cout << " instr_id: " << WQ.entry[index].instr_id
                   << " prior_id: " << MSHR.entry[mshr_index].instr_id;
              cout << " address: " << hex << WQ.entry[index].address;
              cout << " full_addr: " << WQ.entry[index].full_addr << dec;
              cout << " cycle: " << WQ.entry[index].event_cycle << endl;
            });
          } else { // WE SHOULD NOT REACH HERE
            cerr << "[" << NAME << "] MSHR errors" << endl;
            assert(0);
          }
        }

        if (miss_handled) {

          MISS[WQ.entry[index].type]++;
          ACCESS[WQ.entry[index].type]++;

          // remove this entry from WQ
          WQ.remove_queue(&WQ.entry[index]);
        }
      }

      else {
        //* This block will only be executed with L1D, L1I, L2C, LLC not
        // L0D/L0I.

        // find victim
        uint32_t set = get_set(WQ.entry[index].address), way;
        if (cache_type == IS_LLC) {
          way =
              llc_find_victim(writeback_cpu, WQ.entry[index].instr_id, set,
                              block[set], WQ.entry[index].ip,
                              WQ.entry[index].full_addr, WQ.entry[index].type);
        } else
          way = find_victim(writeback_cpu, WQ.entry[index].instr_id, set,
                            block[set], WQ.entry[index].ip,
                            WQ.entry[index].full_addr, WQ.entry[index].type);

#ifdef LLC_BYPASS
        if ((cache_type == IS_LLC) && (way == LLC_WAY)) {
          cerr << "LLC bypassing for writebacks is not allowed!" << endl;
          assert(0);
        }
#endif

        uint8_t do_fill = 1;

        // is this dirty?
        if (block[set][way].dirty) {

          // check if the lower level WQ has enough room to keep this writeback
          // request
          if (lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) ==
                lower_level->get_size(2, block[set][way].address)) {
              // lower level WQ is full, cannot replace this victim

              do_fill = 0;
              lower_level->increment_WQ_FULL(block[set][way].address);
              STALL[WQ.entry[index].type]++;

              DP(if (warmup_complete[writeback_cpu]) {
                cout << "[" << NAME << "] " << __func__
                     << "do_fill: " << +do_fill;
                cout << " lower level wq is full!"
                     << " fill_addr: " << hex << WQ.entry[index].address;
                cout << " victim_addr: " << block[set][way].tag << dec << endl;
              });
            } else {
              PACKET writeback_packet;

              writeback_packet.is_speculative = 0;
              writeback_packet.fill_level = fill_level << 1;
              writeback_packet.cpu = writeback_cpu;
              writeback_packet.address = block[set][way].address;
              writeback_packet.full_addr = block[set][way].full_addr;
              writeback_packet.data = block[set][way].data;
              writeback_packet.instr_id = WQ.entry[index].instr_id;
              writeback_packet.ip = 0;
              writeback_packet.type = WRITEBACK;
              writeback_packet.event_cycle = current_core_cycle[writeback_cpu];

              lower_level->add_wq(&writeback_packet);
            }
          }
#ifdef SANITY_CHECK
          else {
            // sanity check
            if (cache_type != IS_STLB)
              assert(0);
          }
#endif
        }

        if (do_fill) {

          // update prefetcher
          // cache_fill() function is allowed to execute since this exeution
          // will be during the non-speculative execution itself because
          // writebacks are for committed blocks only.

#ifdef SPEC_COMMIT_L1I
          if (cache_type == IS_L0I)
            l1i_prefetcher_cache_fill(
                writeback_cpu,
                ((WQ.entry[index].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE,
                set, way, 0,
                ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);

          if (cache_type == IS_L1I)
            l1i_prefetcher_cache_fill(
                writeback_cpu,
                ((WQ.entry[index].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE,
                set, way, 0,
                ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);
#endif
#ifndef SPEC_COMMIT_L1I
          // blocks are only filled in the L1I cache
          if (cache_type == IS_L1I)
            l1i_prefetcher_cache_fill(
                writeback_cpu,
                ((WQ.entry[index].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE,
                set, way, 0,
                ((block[set][way].ip) >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);
#endif
#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
          if (cache_type == IS_L0D)
            l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0,
                                      block[set][way].address
                                          << LOG2_BLOCK_SIZE,
                                      WQ.entry[index].pf_metadata);

          if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0,
                                      block[set][way].address
                                          << LOG2_BLOCK_SIZE,
                                      WQ.entry[index].pf_metadata);
#endif
#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
          if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0,
                                      block[set][way].address
                                          << LOG2_BLOCK_SIZE,
                                      WQ.entry[index].pf_metadata);
#endif
          else if (cache_type == IS_L2C)
            WQ.entry[index].pf_metadata = l2c_prefetcher_cache_fill(
                WQ.entry[index].address << LOG2_BLOCK_SIZE, set, way, 0,
                block[set][way].address << LOG2_BLOCK_SIZE,
                WQ.entry[index].pf_metadata);

          // update replacement policy
          if (cache_type == IS_LLC) {
            llc_update_replacement_state(
                writeback_cpu, set, way, WQ.entry[index].full_addr,
                WQ.entry[index].ip, block[set][way].full_addr,
                WQ.entry[index].type, 0);
          } else
            update_replacement_state(
                writeback_cpu, set, way, WQ.entry[index].full_addr,
                WQ.entry[index].ip, block[set][way].full_addr,
                WQ.entry[index].type, 0);

          // COLLECT STATS
          sim_miss[writeback_cpu][WQ.entry[index].type]++;
          sim_access[writeback_cpu][WQ.entry[index].type]++;

          // fill the Write block in the cache
          fill_cache(set, way, &WQ.entry[index]);

          // mark dirty
          block[set][way].dirty = 1;

          if (WQ.entry[index].prefetch == 1) {
            block[set][way].prefetch = 1;
          }

          // check fill level
          if (WQ.entry[index].fill_level < fill_level) {
            if (fill_level == FILL_L2) {
              if (WQ.entry[index].fill_l1i) {
                upper_level_icache[writeback_cpu]->return_data(
                    &WQ.entry[index]);
              }
              if (WQ.entry[index].fill_l1d) {
                upper_level_dcache[writeback_cpu]->return_data(
                    &WQ.entry[index]);
              }
            } else if (fill_level == FILL_L1) {
              if (WQ.entry[index].fill_l0i) {
                upper_level_icache[writeback_cpu]->return_data(
                    &WQ.entry[index]);
              }
              if (WQ.entry[index].fill_l0d) {
                upper_level_dcache[writeback_cpu]->return_data(
                    &WQ.entry[index]);
              }
            } else {
              if (WQ.entry[index].instruction)
                upper_level_icache[writeback_cpu]->return_data(
                    &WQ.entry[index]);

              if (WQ.entry[index].is_data)
                upper_level_dcache[writeback_cpu]->return_data(
                    &WQ.entry[index]);
            }
          }

          MISS[WQ.entry[index].type]++;
          ACCESS[WQ.entry[index].type]++;

          // remove this entry from WQ
          WQ.remove_queue(&WQ.entry[index]);
        }
      }
    }
  }
}

void CACHE::handle_read() {
  // handle read

  for (uint32_t i = 0; i < MAX_READ; i++) {

    uint32_t read_cpu = RQ.entry[RQ.head].cpu;

#ifndef NS_INST_PRIORITY
    if (read_cpu == NUM_CPUS)
      return;
#endif

#ifdef NS_INST_PRIORITY
    // Implementation for higher priority to non-speculative blocks

    uint32_t spec_cpu = MSHR_SPEC_BUFFER.entry[MSHR_SPEC_BUFFER.head].cpu;
    if (read_cpu == NUM_CPUS && spec_cpu == NUM_CPUS)
      return;

    // if the buffer contains a packet
    // check if the RQ contains a NS packet (specifically) at the head
    // allow the packet to be processed whichever has the older instruction ID.

    bool process_mshr_buffer = false;

    if (MSHR_SPEC_BUFFER.occupancy > 0) {

      if ((RQ.occupancy > 0) &&
          (RQ.entry[RQ.head].event_cycle <= current_core_cycle[read_cpu]) &&
          (RQ.entry[RQ.head].is_speculative == 0)) {
        if (RQ.entry[RQ.head].instr_id <
            MSHR_SPEC_BUFFER.entry[MSHR_SPEC_BUFFER.head].instr_id) {
          // Packet in RQ is NS and older than MSHR_SPEC_BUFFER -> Process the
          // packet in RQ
          process_mshr_buffer = false;
        } else {
          // Process the packet in MSHR_SPEC_BUFFER
          process_mshr_buffer = true;
        }
      } else {
        process_mshr_buffer = true;
      }
    } else {
      // No entry in MSHR Spec Buffer -> Process the RQ packet
      process_mshr_buffer = false;
    }

    // Process for the packet in MSHR_SPEC_BUFFER
    if (process_mshr_buffer == true) {

      uint32_t buffer_index = MSHR_SPEC_BUFFER.head;

      // This packet can initially be from RQ or PQ
      if (MSHR_SPEC_BUFFER.entry[MSHR_SPEC_BUFFER.head].type == LOAD) {

        // access cache
        uint32_t set = get_set(MSHR_SPEC_BUFFER.entry[buffer_index].address);
        int way = check_hit(&MSHR_SPEC_BUFFER.entry[buffer_index]);

        if (way >= 0) { // read hit

          if (cache_type == IS_L0I || cache_type == IS_L0D) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&MSHR_SPEC_BUFFER.entry[buffer_index]);
          }

          // updation to replacement states
          if (cache_type == IS_LLC) {
            llc_update_replacement_state(
                spec_cpu, set, way, block[set][way].full_addr,
                MSHR_SPEC_BUFFER.entry[buffer_index].ip, 0,
                MSHR_SPEC_BUFFER.entry[buffer_index].type, 1);
          } else
            update_replacement_state(
                spec_cpu, set, way, block[set][way].full_addr,
                MSHR_SPEC_BUFFER.entry[buffer_index].ip, 0,
                MSHR_SPEC_BUFFER.entry[buffer_index].type, 1);

          // check fill level
          if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level < fill_level) {

            if (fill_level == FILL_L2) {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1i) {
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1d) {
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
            } else if (fill_level == FILL_L1) {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0i) {
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0d) {
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
            } else {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].instruction)
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              if (MSHR_SPEC_BUFFER.entry[buffer_index].is_data)
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
            }
          }

          if (block[set][way].prefetch) {
            // this condition will not arise
            prefetched_block_hit_mshr_buffer++;
          }

          block[set][way].used = 1;

          // remove this entry from MSHR SPEC BUFFER
          MSHR_SPEC_BUFFER.remove_queue(&MSHR_SPEC_BUFFER.entry[buffer_index]);
          reads_available_this_cycle--;
        }

        else { // read miss

          // check mshr
          uint8_t miss_handled = 1;
          int mshr_index = check_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);

          if ((mshr_index == -1) &&
              (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

            if (cache_type == IS_LLC) {
              // check to make sure the DRAM RQ has room for this LLC read miss
              if (lower_level->get_occupancy(
                      1, MSHR_SPEC_BUFFER.entry[buffer_index].address) ==
                  lower_level->get_size(
                      1, MSHR_SPEC_BUFFER.entry[buffer_index].address)) {
                miss_handled = 0;
              } else {
                add_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);
                if (lower_level) {
                  //* This executes the --> MEMORY_CONTROLLER   ::
                  // add_rq(PACKET)
                  lower_level->add_rq(&MSHR_SPEC_BUFFER.entry[buffer_index]);
                }
              }
            } else {
              // add it to mshr (read miss)
              add_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);

              // add it to the next level's read queue
              if (lower_level) {
                int is_added =
                    lower_level->add_rq(&MSHR_SPEC_BUFFER.entry[buffer_index]);

                if (is_added == -2) {
                  // lower level RQ is Full
                  assert(0);
                }
                if (is_added == -3) {
                  lower_level->wq_has_demand_block++;
                }
              }
            }
          } else {
            if ((mshr_index == -1) &&
                (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
              // cannot handle miss request until one of MSHRs is available
              miss_handled = 0;
            } else if (mshr_index != -1) { // already in-flight miss

              if (MSHR_SPEC_BUFFER.entry[buffer_index].instruction) {
                uint32_t rob_index =
                    MSHR_SPEC_BUFFER.entry[buffer_index].rob_index;
                MSHR.entry[mshr_index].instruction =
                    1; // add as instruction type
                MSHR.entry[mshr_index].instr_merged = 1;
                MSHR.entry[mshr_index].rob_index_depend_on_me.insert(rob_index);

                if (MSHR_SPEC_BUFFER.entry[buffer_index].instr_merged) {
                  MSHR.entry[mshr_index].rob_index_depend_on_me.join(
                      MSHR_SPEC_BUFFER.entry[buffer_index]
                          .rob_index_depend_on_me,
                      ROB_SIZE);
                }
              } else {
                uint32_t lq_index =
                    MSHR_SPEC_BUFFER.entry[buffer_index].lq_index;
                MSHR.entry[mshr_index].is_data = 1; // add as data type
                MSHR.entry[mshr_index].load_merged = 1;
                MSHR.entry[mshr_index].lq_index_depend_on_me.insert(lq_index);

                MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                    MSHR_SPEC_BUFFER.entry[buffer_index].lq_index_depend_on_me,
                    LQ_SIZE);

                if (MSHR_SPEC_BUFFER.entry[buffer_index].store_merged) {
                  MSHR.entry[mshr_index].store_merged = 1;
                  MSHR.entry[mshr_index].sq_index_depend_on_me.join(
                      MSHR_SPEC_BUFFER.entry[buffer_index]
                          .sq_index_depend_on_me,
                      SQ_SIZE);
                }
              }
              // update fill_level
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level <
                  MSHR.entry[mshr_index].fill_level)
                MSHR.entry[mshr_index].fill_level =
                    MSHR_SPEC_BUFFER.entry[buffer_index].fill_level;

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1i) &&
                  (MSHR.entry[mshr_index].fill_l1i != 1)) {
                MSHR.entry[mshr_index].fill_l1i = 1;
              }

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1d) &&
                  (MSHR.entry[mshr_index].fill_l1d != 1)) {
                MSHR.entry[mshr_index].fill_l1d = 1;
              }

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0i) &&
                  (MSHR.entry[mshr_index].fill_l0i != 1)) {
                MSHR.entry[mshr_index].fill_l0i = 1;
              }

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0d) &&
                  (MSHR.entry[mshr_index].fill_l0d != 1)) {
                MSHR.entry[mshr_index].fill_l0d = 1;
              }
            }
          }

          if (miss_handled) {
            // remove this entry from MSHR_SPEC_BUFFER
            MSHR_SPEC_BUFFER.remove_queue(
                &MSHR_SPEC_BUFFER.entry[buffer_index]);
            reads_available_this_cycle--;
          }
        }
      }

      else if (MSHR_SPEC_BUFFER.entry[MSHR_SPEC_BUFFER.head].type == PREFETCH) {

        // access cache
        uint32_t set = get_set(MSHR_SPEC_BUFFER.entry[buffer_index].address);
        int way = check_hit(&MSHR_SPEC_BUFFER.entry[buffer_index]);

        if (way >= 0) { // prefetch hit

          if (cache_type == IS_LLC) {
            llc_update_replacement_state(
                spec_cpu, set, way, block[set][way].full_addr,
                MSHR_SPEC_BUFFER.entry[buffer_index].ip, 0,
                MSHR_SPEC_BUFFER.entry[buffer_index].type, 1);
          } else {
            update_replacement_state(
                spec_cpu, set, way, block[set][way].full_addr,
                MSHR_SPEC_BUFFER.entry[buffer_index].ip, 0,
                MSHR_SPEC_BUFFER.entry[buffer_index].type, 1);
          }

          // check fill level
          if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level < fill_level) {

            if (fill_level == FILL_L2) {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1i) {
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1d) {
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
            }

            else if (fill_level == FILL_L1) {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0i) {
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0d) {
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
              }
            } else {
              if (MSHR_SPEC_BUFFER.entry[buffer_index].instruction)
                upper_level_icache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);

              if (MSHR_SPEC_BUFFER.entry[buffer_index].is_data)
                upper_level_dcache[spec_cpu]->return_data(
                    &MSHR_SPEC_BUFFER.entry[buffer_index]);
            }
          }

          // remove from MSHR_SPEC_BUFFER
          MSHR_SPEC_BUFFER.remove_queue(&MSHR_SPEC_BUFFER.entry[buffer_index]);
          reads_available_this_cycle--;
        }

        else { // prefetch miss

          // check mshr
          uint8_t miss_handled = 1;
          int mshr_index = check_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);

          if ((mshr_index == -1) &&
              (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

            if (lower_level) {

              if (cache_type == IS_LLC) {

                // check occupancy of DRAM
                if (lower_level->get_occupancy(
                        1, MSHR_SPEC_BUFFER.entry[buffer_index].address) ==
                    lower_level->get_size(
                        1, MSHR_SPEC_BUFFER.entry[buffer_index].address)) {
                  miss_handled = 0;
                } else {
                  // add it to MSHRs if this prefetch miss will be filled to
                  // this cache level
                  if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level <=
                      fill_level)
                    add_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);

                  lower_level->add_rq(
                      &MSHR_SPEC_BUFFER
                           .entry[buffer_index]); // add it to the DRAM RQ
                }
              } else {
                if (lower_level->get_occupancy(
                        3, MSHR_SPEC_BUFFER.entry[buffer_index].address) ==
                    lower_level->get_size(
                        3, MSHR_SPEC_BUFFER.entry[buffer_index].address)) {
                  miss_handled = 0;
                } else {
                  if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level <=
                      fill_level)
                    add_mshr(&MSHR_SPEC_BUFFER.entry[buffer_index]);

                  lower_level->add_pq(&MSHR_SPEC_BUFFER.entry[buffer_index]);
                }
              }
            }
          } else {
            if ((mshr_index == -1) &&
                (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
              miss_handled = 0;
            } else if (mshr_index != -1) { // already in-flight miss

              // no need to update request except fill_level
              // update fill_level
              if (MSHR_SPEC_BUFFER.entry[buffer_index].fill_level <
                  MSHR.entry[mshr_index].fill_level)
                MSHR.entry[mshr_index].fill_level =
                    MSHR_SPEC_BUFFER.entry[buffer_index].fill_level;

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1i) &&
                  (MSHR.entry[mshr_index].fill_l1i != 1)) {
                MSHR.entry[mshr_index].fill_l1i = 1;
              }
              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l1d) &&
                  (MSHR.entry[mshr_index].fill_l1d != 1)) {
                MSHR.entry[mshr_index].fill_l1d = 1;
              }

              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0i) &&
                  (MSHR.entry[mshr_index].fill_l0i != 1)) {
                MSHR.entry[mshr_index].fill_l0i = 1;
              }
              if ((MSHR_SPEC_BUFFER.entry[buffer_index].fill_l0d) &&
                  (MSHR.entry[mshr_index].fill_l0d != 1)) {
                MSHR.entry[mshr_index].fill_l0d = 1;
              }
            } else {
              // Should not reach here.
              assert(0);
            }
          }

          if (miss_handled) {
            // remove this entry from MSHR_SPEC_BUFFER
            MSHR_SPEC_BUFFER.remove_queue(
                &MSHR_SPEC_BUFFER.entry[buffer_index]);
            reads_available_this_cycle--;
          }
        }
      }

      else {
        // No other type is possible in the MSHR buffer other than LOAD and
        // PREFETCH
        assert(0);
      }
    }

#endif

#ifdef NS_INST_PRIORITY
    // Process packet in the RQ
    if (process_mshr_buffer == false) {
#endif

      // handle the oldest entry
      if ((RQ.entry[RQ.head].event_cycle <= current_core_cycle[read_cpu]) &&
          (RQ.occupancy > 0)) {

        int index = RQ.head;

        // access cache
        uint32_t set = get_set(RQ.entry[index].address);
        int way = check_hit(&RQ.entry[index]);

        if (way >= 0) { // read hit

          DPT(if (warmup_complete[read_cpu]) {
            if (cache_type == IS_L0I || cache_type == IS_L0D) {
              cout << "[" << NAME << "] " << __func__
                   << " adding to PROCESSED queue if not of PREFETCH type;  "
                      "current: "
                   << current_core_cycle[read_cpu] << endl;
            }
          });

          if (cache_type == IS_ITLB) {
            RQ.entry[index].instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&RQ.entry[index]);
          } else if (cache_type == IS_DTLB) {
            RQ.entry[index].data_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&RQ.entry[index]);
          } else if (cache_type == IS_STLB)
            RQ.entry[index].data = block[set][way].data;

          else if (cache_type == IS_L0I && (RQ.entry[index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&RQ.entry[index]);
          }

          else if ((cache_type == IS_L0D) &&
                   (RQ.entry[index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
              PROCESSED.add_queue(&RQ.entry[index]);
          }

          // update prefetcher on load instruction
          if (RQ.entry[index].type == LOAD) {

            DPT(if (warmup_complete[read_cpu]) {
              cout << "[" << NAME << "] " << __func__
                   << " executing prefetcher_operate on type == LOAD;";
              cout << " instr_id: " << RQ.entry[index].instr_id
                   << " address: " << hex << RQ.entry[index].address << dec
                   << " full_addr: ";
              cout << hex << RQ.entry[index].full_addr << dec
                   << " is_instruction: "
                   << uint32_t(RQ.entry[index].instruction);
              cout << " is_data " << RQ.entry[index].data;
              cout << " event_cycle: " << RQ.entry[index].event_cycle
                   << "  current: " << current_core_cycle[read_cpu] << endl;
            });

#ifdef MUONTRAP_PREFETCH_ON_COMMIT

            // Speculative / On-Commit Prefetching

            if (RQ.entry[index].is_speculative == 1) {
              // save the parameters for the prefetcher in a map and then call
              // on-commit.

#ifdef PREFETCH_ON_COMMIT_L1I
              // only LOAD requests at L1I can invoke the prefetcher
              if (cache_type == IS_L1I) {

                L1I_PREFETCHER_PARAMS prefetcher_params;

                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.prefetch_hit = block[set][way].prefetch;
                prefetcher_params.cpu = read_cpu;

                ooo_cpu[read_cpu].prefetcher_map_L1I[RQ.entry[index].instr_id] =
                    prefetcher_params;

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__
                       << " saving prefetcher operate call params in the map;"
                       << endl;
                });
              }
#endif

#ifdef SPEC_COMMIT_L1I

              if (cache_type == IS_L0I) {
                spec_fnlmma_call++;

                L1I_PREFETCHER_PARAMS prefetcher_params;

                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.prefetch_hit = block[set][way].prefetch;
                prefetcher_params.cpu = read_cpu;

                ooo_cpu[read_cpu].prefetcher_map_L1I[RQ.entry[index].instr_id] =
                    prefetcher_params;

                l1i_map_size++;
                if (l1i_map_size > l1i_max_map_size)
                  l1i_max_map_size = l1i_map_size;

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__
                       << " saving prefetcher operate call params in the map;"
                       << endl;
                  cout << "[" << NAME << "] " << __func__
                       << " speculative call to prefetcher  speculative_bit: 1"
                       << endl;
                });

                // invoke prefetcher
                l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 1,
                                             block[set][way].prefetch, 1);
              }

#endif

#ifdef PREFETCH_ON_COMMIT_L1D
              if (cache_type == IS_L1D) {

                PREFETCHER_REQUEST_PARAMS prefetcher_params;

                prefetcher_params.addr = RQ.entry[index].full_addr;
                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.type = RQ.entry[index].type;
                prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                prefetcher_params.cpu = read_cpu;

                ooo_cpu[read_cpu].prefetcher_map_L1D[RQ.entry[index].instr_id] =
                    prefetcher_params;

                l1d_map_size++;
                if (l1d_map_size > l1d_max_map_size)
                  l1d_max_map_size = l1d_map_size;

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__
                       << " saving prefetcher operate call params in the map;"
                       << endl;
                });
              }
#endif

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)

              if (cache_type == IS_L0D) {

                PREFETCHER_REQUEST_PARAMS prefetcher_params;

                prefetcher_params.addr = RQ.entry[index].full_addr;
                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.type = RQ.entry[index].type;
                prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                prefetcher_params.cpu = read_cpu;

                ooo_cpu[read_cpu].prefetcher_map_L1D[RQ.entry[index].instr_id] =
                    prefetcher_params;

                l1d_map_size++;
                if (l1d_map_size > l1d_max_map_size)
                  l1d_max_map_size = l1d_map_size;

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__
                       << " saving prefetcher operate call params in the map;"
                       << endl;
                  cout << "[" << NAME << "] " << __func__
                       << " speculative call to prefetcher  speculative_bit: 1"
                       << endl;
                });

                // invoke prefetcher
                if (speculative_prefetch == 1) {
                  l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                         RQ.entry[index].ip, 1,
                                         RQ.entry[index].type, 1);
                }
              }
#endif

#ifdef PREFETCH_ON_COMMIT_L2C
              if (cache_type == IS_L2C) {
                L2C_PREFETCHER_REQUEST_PARAMS prefetcher_params;

                prefetcher_params.addr = block[set][way].address
                                         << LOG2_BLOCK_SIZE;
                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.type = RQ.entry[index].type;
                prefetcher_params.metadata_in = 0;
                prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                prefetcher_params.cpu = read_cpu;

                prefetcher_params.from_handle_prefetch = 0;

                ooo_cpu[read_cpu].prefetcher_map_L2C[RQ.entry[index].instr_id] =
                    prefetcher_params;
              }
#endif

#ifdef SPEC_COMMIT_L2C
              else if (cache_type == IS_L2C) {
                L2C_PREFETCHER_REQUEST_PARAMS prefetcher_params;

                prefetcher_params.addr = block[set][way].address
                                         << LOG2_BLOCK_SIZE;
                prefetcher_params.ip = RQ.entry[index].ip;
                prefetcher_params.cache_hit = 1;
                prefetcher_params.type = RQ.entry[index].type;
                prefetcher_params.metadata_in = 0;
                prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                prefetcher_params.cpu = read_cpu;

                prefetcher_params.from_handle_prefetch = 0;

                ooo_cpu[read_cpu].prefetcher_map_L2C[RQ.entry[index].instr_id] =
                    prefetcher_params;

                l2c_prefetcher_operate(
                    block[set][way].address << LOG2_BLOCK_SIZE,
                    RQ.entry[index].ip, 1, RQ.entry[index].type, 0, 1);
              }
#endif
            }

            else if (RQ.entry[index].is_speculative == 0) {
              // allow the call to prefetcher operate.

              DPT(if (warmup_complete[read_cpu]) {
                cout << "[" << NAME << "] " << __func__
                     << " committed call to prefetcher operate; "
                        "speculative_bit: 0"
                     << endl;
              });

#ifdef SPEC_COMMIT_L1I
              if (cache_type == IS_L0I) {
                non_spec_fnlmma_call++;
                l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 1,
                                             block[set][way].prefetch, 0);
              }
#endif
#ifndef SPEC_COMMIT_L1I
              if (cache_type == IS_L1I) {
                l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 1,
                                             block[set][way].prefetch);
              }
#endif

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
              if (cache_type == IS_L0D) {
                l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                       RQ.entry[index].ip, 1,
                                       RQ.entry[index].type, 0);
              }
#endif
#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
              if (cache_type == IS_L1D) {
                l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                       RQ.entry[index].ip, 1,
                                       RQ.entry[index].type);
              }
#endif

              else if (cache_type == IS_L2C) {
                l2c_prefetcher_operate(
                    block[set][way].address << LOG2_BLOCK_SIZE,
                    RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
              }
            }

#endif

#ifndef MUONTRAP_PREFETCH_ON_COMMIT

            // Baseline (unsafe) prefetching

            // Directly invoking the prefetcher even if it was speculative
            // execution. This will bring the prefetched blocks even if the
            // instruction is not committed.

            if (cache_type == IS_L1I) {
              l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 1,
                                           block[set][way].prefetch);
            }

            if (cache_type == IS_L1D) {
              l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                     RQ.entry[index].ip, 1,
                                     RQ.entry[index].type);
            }

            else if (cache_type == IS_L2C) {
              l2c_prefetcher_operate(block[set][way].address << LOG2_BLOCK_SIZE,
                                     RQ.entry[index].ip, 1,
                                     RQ.entry[index].type, 0);
            }

#endif
          }

          if (cache_type == IS_LLC) {
            llc_update_replacement_state(
                read_cpu, set, way, block[set][way].full_addr,
                RQ.entry[index].ip, 0, RQ.entry[index].type, 1);
          } else
            update_replacement_state(
                read_cpu, set, way, block[set][way].full_addr,
                RQ.entry[index].ip, 0, RQ.entry[index].type, 1);

          // COLLECT STATS
          sim_hit[read_cpu][RQ.entry[index].type]++;
          sim_access[read_cpu][RQ.entry[index].type]++;

          // check fill level
          if (RQ.entry[index].fill_level < fill_level) {

            if (fill_level == FILL_L2) {
              if (RQ.entry[index].fill_l1i) {
                upper_level_icache[read_cpu]->return_data(&RQ.entry[index]);
              }
              if (RQ.entry[index].fill_l1d) {
                upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
              }
            } else if (fill_level == FILL_L1) { //* L0D_CACHE   L0I_CACHE
              if (RQ.entry[index].fill_l0i) {
                upper_level_icache[read_cpu]->return_data(&RQ.entry[index]);
              }
              if (RQ.entry[index].fill_l0d) {
                upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
              }
            } else {
              if (RQ.entry[index].instruction) {
                for (int k = 0; k < NUM_CPUS; k++) {
                  upper_level_icache[k]->return_data(&RQ.entry[index]);
                }
              }

              if (RQ.entry[index].is_data) {
                for (int k = 0; k < NUM_CPUS; k++) {
                  upper_level_dcache[k]->return_data(&RQ.entry[index]);
                }
              }
            }
          }

          // allowed during speculative and committed execution because
          // USEFULNESS of the prefetch block has to be monitored using this
          // update prefetch stats and reset prefetch bit
          if (block[set][way].prefetch) {

            //* Usefulness counters are updated in this block

            DPT(if (warmup_complete[read_cpu]) {
              cout << "[" << NAME << "] " << __func__
                   << " prefetched block useful++ ";
              cout << " instr_id: " << RQ.entry[index].instr_id;
              cout << " address: " << hex << RQ.entry[index].address << dec;
              cout << " full_addr: " << hex << RQ.entry[index].full_addr << dec;
              cout << " is_instruction: "
                   << uint32_t(RQ.entry[index].instruction);
              cout << " is_data " << RQ.entry[index].data;
              cout << " event_cycle: " << RQ.entry[index].event_cycle;
              cout << "  current: " << current_core_cycle[read_cpu] << endl;
              cout << endl;
            });

            pf_useful++;
            pf_useful_2++;

            if (cache_type == IS_L0D || cache_type == IS_L1D) {
              useful_both++;
            }
            if (cache_type == IS_L1D) {
              useful_l1d++;
            }

#ifdef IPCP_PREFETCHER_NEW
            // for IPCP.
            if (cache_type == IS_L0D || cache_type == IS_L1D) {
              IPCP_OFFSET_USEFUL_COUNT[block[set][way].ipcp_offset]++;
            }
#endif

#ifdef FNLMMA
            // stats for FNL
            // Increment the usefulness counter for FNL and MMA component
            if (cache_type == IS_L1I || cache_type == IS_L0I) {

              if (block[set][way].is_FNL == 0)
                pf_MMA_useful++;
              else if (block[set][way].is_FNL == 1)
                pf_FNL_useful++;
              else if (block[set][way].is_FNL == 2)
                not_defined_useful++;
              else if (block[set][way].is_FNL == 3)
                pf_Temporal_useful++;
            }
#endif

            block[set][way].prefetch = 0;

#ifdef IPCP_PREFETCHER
            // for IPCP Throttling.
            if (block[set][way].pref_class < 5)
              pref_useful[cpu][block[set][way].pref_class]++;
#endif
          }

          block[set][way].used = 1;

          HIT[RQ.entry[index].type]++;
          ACCESS[RQ.entry[index].type]++;

          // remove this entry from RQ
          RQ.remove_queue(&RQ.entry[index]);

          reads_available_this_cycle--;
        }

        else { // read miss

          DPT(if (warmup_complete[read_cpu]) {
            cout << "[" << NAME << "] " << __func__ << " read miss";
            cout << " instr_id: " << RQ.entry[index].instr_id
                 << " address: " << hex << RQ.entry[index].address;
            cout << " full_addr: " << RQ.entry[index].full_addr << dec;
            cout << " cycle: " << RQ.entry[index].event_cycle << endl;
          });

          // check mshr
          uint8_t miss_handled = 1;
          int mshr_index = check_mshr(&RQ.entry[index]);

          if (mshr_index == -2) // data/instruction collision in MSHR
          {
            // this is a data/instruction collision in the MSHR, so we have to
            // wait before we can allocate this miss
            miss_handled = 0;
          }

          else if ((mshr_index == -1) &&
                   (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

            if (cache_type == IS_LLC) {
              // check to make sure the DRAM RQ has room for this LLC read miss
              if (lower_level->get_occupancy(1, RQ.entry[index].address) ==
                  lower_level->get_size(1, RQ.entry[index].address)) {
                miss_handled = 0;
              } else {
                add_mshr(&RQ.entry[index]);
                if (lower_level) {
                  // executes the --> MEMORY_CONTROLLER::add_rq(PACKET)
                  lower_level->add_rq(&RQ.entry[index]);
                }
              }
            } else {
              // add it to mshr (read miss)
              add_mshr(&RQ.entry[index]);

              // add it to the next level's read queue
              if (lower_level) {
                int is_added = lower_level->add_rq(&RQ.entry[index]);

                if (cache_type == IS_L2C) {
                  if (lower_level->get_occupancy(1, RQ.entry[index].address) ==
                      lower_level->get_size(1, RQ.entry[index].address)) {
                    //@Sumon: No action, need to fix
                  }
                }

                if (is_added == -3) {
                  lower_level->wq_has_demand_block++;
                }
              }

              else { // this is the last level

                if (cache_type == IS_STLB) {

                  // page table walk
                  uint64_t pa = va_to_pa(read_cpu, RQ.entry[index].instr_id,
                                         RQ.entry[index].full_addr,
                                         RQ.entry[index].address, 0);

                  RQ.entry[index].data = pa >> LOG2_PAGE_SIZE;
                  RQ.entry[index].event_cycle = current_core_cycle[read_cpu];
                  return_data(&RQ.entry[index]);
                }
              }
            }
          }

          else {
            if ((mshr_index == -1) &&
                (MSHR.occupancy ==
                 MSHR_SIZE)) { // not enough MSHR resource

#ifndef NS_INST_PRIORITY
                               // cannot handle miss request until one of MSHRs
                               // is available
              miss_handled = 0;
              STALL[RQ.entry[index].type]++;
#endif

#ifdef NS_INST_PRIORITY

              // TLBs are not considered here.
              if (cache_type == IS_ITLB || cache_type == IS_DTLB ||
                  cache_type == IS_STLB) {
                miss_handled = 0;
                STALL[RQ.entry[index].type]++;
              }

              else if (RQ.entry[index].is_speculative ==
                       0) { // Updating the count variables on a NS RQ packet;

                bool dram_allowed =
                    true; // false doesn't allow further execution

                // Pre-checking if DRAM RQ has romm for LLC read miss
                if (cache_type == IS_LLC) {
                  if (lower_level->get_occupancy(1, RQ.entry[index].address) ==
                      lower_level->get_size(1, RQ.entry[index].address)) {
                    dram_allowed = false;
                    miss_handled = 0;
                  }
                }

                if (dram_allowed == true) {

                  // Traverse through the complete MSHR to find the youngest
                  // speculative instruction.
                  uint64_t youngest_inst_id = 0, youngest_inst_index = 0;

                  for (uint32_t index_temp = 0; index_temp < MSHR_SIZE;
                       index_temp++) {
                    if ((MSHR.entry[index_temp].is_speculative == 1) &&
                        MSHR.entry[index_temp].instr_id >= youngest_inst_id) {
                      youngest_inst_id = MSHR.entry[index_temp].instr_id;
                      youngest_inst_index = index_temp;
                    }
                  }

                  if (youngest_inst_id > 0) {
                    if (MSHR_SPEC_BUFFER.occupancy >= MSHR_SPEC_BUFFER.SIZE) {
                      // MSHR extra buffer is full
                      mshr_spec_buffer_full++;
                      miss_handled = 0;
                    } else {

                      // extra buffer has space
                      int extra_buffer_index = MSHR_SPEC_BUFFER.check_queue(
                          &MSHR.entry[youngest_inst_index]);

                      if (extra_buffer_index != -1) {
                        // entry exists in the MSHR_SPEC_BUFFER
                        // remove the entry from MSHR but don't add to Extra
                        // Buffer
                        MSHR.remove_queue(&MSHR.entry[youngest_inst_index]);
                      } else {
                        // add entry to extra buffer and remove from MSHR
                        MSHR_SPEC_BUFFER.add_queue(
                            &MSHR.entry[youngest_inst_index]);
                        MSHR.remove_queue(&MSHR.entry[youngest_inst_index]);
                      }

                      // add Non-speculative RQ entry to MSHR+lower_level(RQ)
                      // and remove from RQ

                      if (cache_type == IS_LLC) {
                        add_mshr(&RQ.entry[index]);
                        if (lower_level) {
                          // executes the --> MEMORY_CONTROLLER   ::
                          // add_rq(PACKET)
                          lower_level->add_rq(&RQ.entry[index]);
                        }
                      } else {

                        add_mshr(&RQ.entry[index]);

                        if (lower_level) {
                          int is_added = lower_level->add_rq(&RQ.entry[index]);

                          if (is_added == -2) {
                            // lower level RQ full
                            assert(0);
                          } else if (is_added == -3) {
                            lower_level->wq_has_demand_block++;
                          }
                        }
                      }
                    }
                  } else {
                    // No younger instruction. Leave.
                    miss_handled = 0;
                  }
                }

                if (miss_handled == 0) {
                  STALL[RQ.entry[index].type]++;
                }

                if (cache_type == IS_L0I) {
                  mshr_full_l0i++;
                } else if (cache_type == IS_L0D) {
                  mshr_full_l0d++;
                } else if (cache_type == IS_L1I) {
                  mshr_full_l1i++;
                } else if (cache_type == IS_L1D) {
                  mshr_full_l1d++;
                } else if (cache_type == IS_L2C) {
                  mshr_full_l2c++;
                } else if (cache_type == IS_LLC) {
                  mshr_full_llc++;
                }
              } else {
                // speculative instruction at RQ head
                miss_handled = 0;
                STALL[RQ.entry[index].type]++;
              }
#endif
            }

            else if (mshr_index != -1) { // already in-flight miss

              // mark merged consumer
              if (RQ.entry[index].type == RFO) {

                if (RQ.entry[index].tlb_access) {
                  uint32_t sq_index = RQ.entry[index].sq_index;
                  MSHR.entry[mshr_index].store_merged = 1;
                  MSHR.entry[mshr_index].sq_index_depend_on_me.insert(sq_index);
                  MSHR.entry[mshr_index].sq_index_depend_on_me.join(
                      RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                }

                if (RQ.entry[index].load_merged) {
                  // uint32_t lq_index = RQ.entry[index].lq_index;
                  MSHR.entry[mshr_index].load_merged = 1;
                  // MSHR.entry[mshr_index].lq_index_depend_on_me[lq_index] = 1;
                  MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                      RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                }
              }

              else {
                if (RQ.entry[index].instruction) {
                  uint32_t rob_index = RQ.entry[index].rob_index;
                  MSHR.entry[mshr_index].instruction =
                      1; // add as instruction type
                  MSHR.entry[mshr_index].instr_merged = 1;
                  MSHR.entry[mshr_index].rob_index_depend_on_me.insert(
                      rob_index);

                  DPT(if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                    cout << "[INSTR_MERGED] " << __func__
                         << " cpu: " << MSHR.entry[mshr_index].cpu
                         << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                    cout << " merged rob_index: " << rob_index
                         << " instr_id: " << RQ.entry[index].instr_id << endl;
                  });

                  if (RQ.entry[index].instr_merged) {
                    MSHR.entry[mshr_index].rob_index_depend_on_me.join(
                        RQ.entry[index].rob_index_depend_on_me, ROB_SIZE);
                    DPT(if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                      cout << "[INSTR_MERGED] " << __func__
                           << " cpu: " << MSHR.entry[mshr_index].cpu
                           << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                      cout << " merged rob_index: " << i << " instr_id: N/A"
                           << endl;
                    });
                  }
                } else {
                  uint32_t lq_index = RQ.entry[index].lq_index;
                  MSHR.entry[mshr_index].is_data = 1; // add as data type
                  MSHR.entry[mshr_index].load_merged = 1;
                  MSHR.entry[mshr_index].lq_index_depend_on_me.insert(lq_index);

                  DPT(if (warmup_complete[read_cpu]) {
                    cout << "[DATA_MERGED] " << __func__ << " cpu: " << read_cpu
                         << " instr_id: " << RQ.entry[index].instr_id;
                    cout << " merged rob_index: " << RQ.entry[index].rob_index
                         << " instr_id: " << RQ.entry[index].instr_id
                         << " lq_index: " << RQ.entry[index].lq_index << endl;
                  });
                  MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                      RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);

                  if (RQ.entry[index].store_merged) {
                    MSHR.entry[mshr_index].store_merged = 1;
                    MSHR.entry[mshr_index].sq_index_depend_on_me.join(
                        RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                  }
                }
              }

              // update fill_level
              if (RQ.entry[index].fill_level <
                  MSHR.entry[mshr_index].fill_level)
                MSHR.entry[mshr_index].fill_level = RQ.entry[index].fill_level;

              if ((RQ.entry[index].fill_l1i) &&
                  (MSHR.entry[mshr_index].fill_l1i != 1)) {
                MSHR.entry[mshr_index].fill_l1i = 1;
              }

              if ((RQ.entry[index].fill_l1d) &&
                  (MSHR.entry[mshr_index].fill_l1d != 1)) {
                MSHR.entry[mshr_index].fill_l1d = 1;
              }

              if ((RQ.entry[index].fill_l0i) &&
                  (MSHR.entry[mshr_index].fill_l0i != 1)) {
                MSHR.entry[mshr_index].fill_l0i = 1;
              }

              if ((RQ.entry[index].fill_l0d) &&
                  (MSHR.entry[mshr_index].fill_l0d != 1)) {
                MSHR.entry[mshr_index].fill_l0d = 1;
              }

              // update request
              if (MSHR.entry[mshr_index].type == PREFETCH) {

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__ << " lateness++ "
                       << " instr_id: " << RQ.entry[index].instr_id;
                  cout << " address: " << hex << RQ.entry[index].address << dec;
                  cout << " full_addr " << hex << RQ.entry[index].full_addr
                       << dec;
                  cout << endl;
                });

                lateness_counter++;

#ifdef FNLMMA
                if (MSHR.entry[mshr_index].is_FNL == 1) {
                  lateness_FNL_counter++;
                } else if (MSHR.entry[mshr_index].is_FNL == 0)
                  lateness_MMA_counter++;
                else if (MSHR.entry[mshr_index].is_FNL == 3)
                  lateness_Temporal_counter++;
#endif

                uint8_t prior_returned = MSHR.entry[mshr_index].returned;
                uint64_t prior_event_cycle = MSHR.entry[mshr_index].event_cycle;

                uint8_t speculative_bit =
                    RQ.entry[index].is_speculative &
                    MSHR.entry[mshr_index]
                        .is_speculative; // if even one of them is committed
                                         // then consider complete as committed
                int fill_level = 0;
                uint8_t fill_l1d = 0, fill_l1i = 0, fill_l0d = 0, fill_l0i = 0;

                if (MSHR.entry[mshr_index].fill_level <
                    RQ.entry[index].fill_level) {
                  fill_level = MSHR.entry[mshr_index].fill_level;
                } else {
                  fill_level = RQ.entry[index].fill_level;
                }
                if (MSHR.entry[mshr_index].fill_l1d ||
                    RQ.entry[index].fill_l1d) {
                  fill_l1d = 1;
                }
                if (MSHR.entry[mshr_index].fill_l1i ||
                    RQ.entry[index].fill_l1i) {
                  fill_l1i = 1;
                }
                if (MSHR.entry[mshr_index].fill_l0d ||
                    RQ.entry[index].fill_l0d) {
                  fill_l0d = 1;
                }
                if (MSHR.entry[mshr_index].fill_l0i ||
                    RQ.entry[index].fill_l0i) {
                  fill_l0i = 1;
                }

                MSHR.entry[mshr_index] = RQ.entry[index];

                // in case request is already returned, we should keep
                // event_cycle and retunred variables
                MSHR.entry[mshr_index].returned = prior_returned;
                MSHR.entry[mshr_index].event_cycle = prior_event_cycle;

                MSHR.entry[mshr_index].is_speculative = speculative_bit;
                MSHR.entry[mshr_index].fill_level = fill_level;
                MSHR.entry[mshr_index].fill_l1d = fill_l1d;
                MSHR.entry[mshr_index].fill_l1i = fill_l1i;
                MSHR.entry[mshr_index].fill_l0d = fill_l0d;
                MSHR.entry[mshr_index].fill_l0i = fill_l0i;

#ifdef IPCP_PREFETCHER
                // stats for IPCP
                if (cache_type == IS_L1D) {
                  MSHR.entry[mshr_index].late_pref = 1;
                }
#endif
              }

              MSHR_MERGED[RQ.entry[index].type]++;

              DPT(if (warmup_complete[read_cpu]) {
                cout << "[" << NAME << "] " << __func__ << " mshr merged";
                cout << " instr_id: " << RQ.entry[index].instr_id
                     << " prior_id: " << MSHR.entry[mshr_index].instr_id;
                cout << " address: " << hex << RQ.entry[index].address;
                cout << " full_addr: " << RQ.entry[index].full_addr << dec;
                cout << " cycle: " << RQ.entry[index].event_cycle << endl;
              });
            }

            else { // WE SHOULD NOT REACH HERE
              cerr << "[" << NAME << "] MSHR errors" << endl;
              assert(0);
            }
          }

          if (miss_handled) {

            //* Update prefetcher on load instruction
            if (RQ.entry[index].type == LOAD) {

              DPT(if (warmup_complete[read_cpu]) {
                cout << "[" << NAME << "] " << __func__
                     << " executing prefetcher_operate on type == LOAD;";
                cout << " instr_id: " << RQ.entry[index].instr_id
                     << " address: " << hex << RQ.entry[index].address << dec
                     << " full_addr: ";
                cout << hex << RQ.entry[index].full_addr << dec
                     << " is_instruction: "
                     << uint32_t(RQ.entry[index].instruction);
                cout << " is_data " << RQ.entry[index].data;
                cout << " event_cycle: " << RQ.entry[index].event_cycle
                     << "  current: " << current_core_cycle[read_cpu] << endl;
              });

#ifdef MUONTRAP_PREFETCH_ON_COMMIT

              if (RQ.entry[index].is_speculative == 1) {
                // save the parameters for the prefetcher in a map and then call
                // on-commit.

#ifdef PREFETCH_ON_COMMIT_L1I
                if (cache_type == IS_L1I) {

                  L1I_PREFETCHER_PARAMS prefetcher_params;

                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.prefetch_hit = 0;
                  prefetcher_params.cpu = read_cpu;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L1I[RQ.entry[index].instr_id] =
                      prefetcher_params;

                  DPT(if (warmup_complete[read_cpu]) {
                    cout << "[" << NAME << "] " << __func__
                         << " saving prefetcher operate call params in the map;"
                         << endl;
                  });
                }
#endif

#ifdef SPEC_COMMIT_L1I
                // L0I invokes the L1I prefetcher
                if (cache_type == IS_L0I) {
                  spec_fnlmma_call++;

                  L1I_PREFETCHER_PARAMS prefetcher_params;

                  prefetcher_params.cpu = read_cpu;
                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.prefetch_hit = 0;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L1I[RQ.entry[index].instr_id] =
                      prefetcher_params;

                  l1i_map_size++;
                  if (l1i_map_size > l1i_max_map_size)
                    l1i_max_map_size = l1i_map_size;

                  DPT(if (warmup_complete[read_cpu]) {
                    cout << "[" << NAME << "] " << __func__
                         << " saving prefetcher operate call params in the map;"
                         << endl;
                    cout << "[" << NAME << "] " << __func__
                         << " speculative call to prefetcher;  "
                            "speculative_bit: 1"
                         << endl;
                  });

                  l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 0,
                                               0, 1);
                }
#endif

#ifdef PREFETCH_ON_COMMIT_L1D
                if (cache_type == IS_L1D) {

                  PREFETCHER_REQUEST_PARAMS prefetcher_params;

                  prefetcher_params.addr = RQ.entry[index].full_addr;
                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.type = RQ.entry[index].type;
                  prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                  prefetcher_params.cpu = read_cpu;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L1D[RQ.entry[index].instr_id] =
                      prefetcher_params;

                  l1d_map_size++;
                  if (l1d_map_size > l1d_max_map_size)
                    l1d_max_map_size = l1d_map_size;
                }
#endif

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
                // L0D invokes the L1D-prefetcher
                if (cache_type == IS_L0D) {

                  PREFETCHER_REQUEST_PARAMS prefetcher_params;

                  prefetcher_params.addr = RQ.entry[index].full_addr;
                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.type = RQ.entry[index].type;
                  prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                  prefetcher_params.cpu = read_cpu;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L1D[RQ.entry[index].instr_id] =
                      prefetcher_params;

                  l1d_map_size++;
                  if (l1d_map_size > l1d_max_map_size)
                    l1d_max_map_size = l1d_map_size;

                  if (speculative_prefetch == 1) {
                    l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                           RQ.entry[index].ip, 0,
                                           RQ.entry[index].type, 1);
                  }
                }
#endif

#ifdef PREFETCH_ON_COMMIT_L2C
                if (cache_type == IS_L2C) {
                  L2C_PREFETCHER_REQUEST_PARAMS prefetcher_params;

                  prefetcher_params.addr = RQ.entry[index].address
                                           << LOG2_BLOCK_SIZE;
                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.type = RQ.entry[index].type;
                  prefetcher_params.metadata_in = 0;
                  prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                  prefetcher_params.cpu = read_cpu;

                  prefetcher_params.from_handle_prefetch = 0;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L2C[RQ.entry[index].instr_id] =
                      prefetcher_params;
                }
#endif

#ifdef SPEC_COMMIT_L2C
                if (cache_type == IS_L2C) {
                  L2C_PREFETCHER_REQUEST_PARAMS prefetcher_params;

                  prefetcher_params.addr = RQ.entry[index].address
                                           << LOG2_BLOCK_SIZE;
                  prefetcher_params.ip = RQ.entry[index].ip;
                  prefetcher_params.cache_hit = 0;
                  prefetcher_params.type = RQ.entry[index].type;
                  prefetcher_params.metadata_in = 0;
                  prefetcher_params.curr_cycle = current_core_cycle[read_cpu];
                  prefetcher_params.cpu = read_cpu;

                  prefetcher_params.from_handle_prefetch = 0;

                  ooo_cpu[read_cpu]
                      .prefetcher_map_L2C[RQ.entry[index].instr_id] =
                      prefetcher_params;

                  l2c_prefetcher_operate(
                      RQ.entry[index].address << LOG2_BLOCK_SIZE,
                      RQ.entry[index].ip, 0, RQ.entry[index].type, 0, 1);
                }
#endif
              }

              else if (RQ.entry[index].is_speculative == 0) {

                DPT(if (warmup_complete[read_cpu]) {
                  cout << "[" << NAME << "] " << __func__
                       << " committed call to prefetcher operate; "
                          "speculative_bit: 0"
                       << endl;
                });

#ifdef SPEC_COMMIT_L1I
                if (cache_type == IS_L0I) {
                  non_spec_fnlmma_call++;
                  l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 0,
                                               0, 0);
                }
#endif
#ifndef SPEC_COMMIT_L1I
                if (cache_type == IS_L1I) {
                  l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 0,
                                               0);
                }
#endif

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
                if (cache_type == IS_L0D) {
                  l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                         RQ.entry[index].ip, 0,
                                         RQ.entry[index].type, 0);
                }
#endif
#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
                if (cache_type == IS_L1D) {
                  l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                         RQ.entry[index].ip, 0,
                                         RQ.entry[index].type);
                }
#endif

                else if (cache_type == IS_L2C) {
                  l2c_prefetcher_operate(
                      RQ.entry[index].address << LOG2_BLOCK_SIZE,
                      RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
                }
              }

#endif

#ifndef MUONTRAP_PREFETCH_ON_COMMIT
              // call the prefetcher operate here itself.

              if (cache_type == IS_L1I) {
                l1i_prefetcher_cache_operate(read_cpu, RQ.entry[index].ip, 0,
                                             0);
              }

              if (cache_type == IS_L1D) {
                l1d_prefetcher_operate(RQ.entry[index].full_addr,
                                       RQ.entry[index].ip, 0,
                                       RQ.entry[index].type);
              }

              else if (cache_type == IS_L2C) {
                l2c_prefetcher_operate(
                    RQ.entry[index].address << LOG2_BLOCK_SIZE,
                    RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
              }
#endif
            }

            MISS[RQ.entry[index].type]++;
            ACCESS[RQ.entry[index].type]++;

            // remove this entry from RQ
            RQ.remove_queue(&RQ.entry[index]);
            reads_available_this_cycle--;
          }
        }
      } else {
        return;
      }

#ifdef NS_INST_PRIORITY
    }
#endif

    if (reads_available_this_cycle == 0) {
      return;
    }
  }
}

void CACHE::handle_prefetch() {
  // handle prefetch

  for (uint32_t i = 0; i < MAX_READ; i++) {

    uint32_t prefetch_cpu = PQ.entry[PQ.head].cpu;
    if (prefetch_cpu == NUM_CPUS)
      return;

    if (PQ.entry[PQ.head].type == COMMIT_LOAD ||
        PQ.entry[PQ.head].type == COMMIT_WRITE) {
      // these types cannot issue prefetches
      assert(0);
    }

    // handle the oldest entry
    if ((PQ.entry[PQ.head].event_cycle <= current_core_cycle[prefetch_cpu]) &&
        (PQ.occupancy > 0)) {
      int index = PQ.head;

      // access cache
      uint32_t set = get_set(PQ.entry[index].address);
      int way = check_hit(&PQ.entry[index]);

      if (way >= 0) { // prefetch hit

        // update replacement policy
        if (cache_type == IS_LLC) {
          llc_update_replacement_state(
              prefetch_cpu, set, way, block[set][way].full_addr,
              PQ.entry[index].ip, 0, PQ.entry[index].type, 1);
        } else
          update_replacement_state(
              prefetch_cpu, set, way, block[set][way].full_addr,
              PQ.entry[index].ip, 0, PQ.entry[index].type, 1);

        if (cache_type == IS_L1D) {
          if (prefetch_cpu == 0 && PQ.entry[index].type == PREFETCH) {
            prefetch_hits_l1d++;
          }
        }

        if (cache_type == IS_L2C) {
          if (prefetch_cpu == 0 && PQ.entry[index].type == PREFETCH) {
            prefetch_hits_l2c++;
          }
        }

        // COLLECT STATS
        sim_hit[prefetch_cpu][PQ.entry[index].type]++;
        sim_access[prefetch_cpu][PQ.entry[index].type]++;

        // Explanation::
        // Run prefetcher on prefetches from higher caches.
        // If there is a L1 and an L2 prefetcher and the request is coming from
        // L1 prefetcher then trigger L2 prefetcher also. Higher means L1, lower
        // means LLC.

        // prefetchers are not being trained again when L0 prefetch request
        // arrives, since those are the prefetch requests generated by
        // themselves
        if ((PQ.entry[index].is_speculative == 0) &&
            PQ.entry[index].pf_origin_level !=
                FILL_L0) // only committed prefetch requests can invoke
                         // prefetchers of lower levels
        {
          if (PQ.entry[index].pf_origin_level < fill_level) {

            DPT(if (warmup_complete[prefetch_cpu]) {
              cout << "[" << NAME << "] " << __func__
                   << " lower level prefetcher_operate call; speculative_bit: 0"
                   << endl;
            });

            //* NOTE: Handle prefetch triggers the prefetch request for the
            // lower level, it does not fill in the cache but rather adds a
            // request in the PQ of the lower level cache prefetcher.

#ifdef SPEC_COMMIT_L2C
            if (cache_type == IS_L2C) {
              handle_prefetch_l2c_call_count++;
              PQ.entry[index].pf_metadata = l2c_prefetcher_operate(
                  block[set][way].address << LOG2_BLOCK_SIZE,
                  PQ.entry[index].ip, 1, PREFETCH, PQ.entry[index].pf_metadata,
                  0);
            }
#endif
#ifndef SPEC_COMMIT_L2C
            if (cache_type == IS_L2C) {
              handle_prefetch_l2c_call_count++;
              PQ.entry[index].pf_metadata = l2c_prefetcher_operate(
                  block[set][way].address << LOG2_BLOCK_SIZE,
                  PQ.entry[index].ip, 1, PREFETCH, PQ.entry[index].pf_metadata);
            }
#endif
          }
        }

        // check fill level
        if (PQ.entry[index].fill_level < fill_level) {

          if (fill_level == FILL_L2) {
            if (PQ.entry[index].fill_l1i) {
              upper_level_icache[prefetch_cpu]->return_data(&PQ.entry[index]);
            }
            if (PQ.entry[index].fill_l1d) {
              upper_level_dcache[prefetch_cpu]->return_data(&PQ.entry[index]);
            }
          }

          else if (fill_level == FILL_L1) {
            if (PQ.entry[index].fill_l0i) {
              upper_level_icache[prefetch_cpu]->return_data(&PQ.entry[index]);
            }
            if (PQ.entry[index].fill_l0d) {
              upper_level_dcache[prefetch_cpu]->return_data(&PQ.entry[index]);
            }
          } else {
            if (PQ.entry[index].instruction) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_icache[k]->return_data(&PQ.entry[index]);
              }
            }

            if (PQ.entry[index].is_data) {
              for (int k = 0; k < NUM_CPUS; k++) {
                upper_level_dcache[k]->return_data(&PQ.entry[index]);
              }
            }
          }
        }

        HIT[PQ.entry[index].type]++;
        ACCESS[PQ.entry[index].type]++;

        // remove this entry from PQ
        PQ.remove_queue(&PQ.entry[index]);
        reads_available_this_cycle--;
      }

      else { // prefetch miss

        DPT(if (warmup_complete[prefetch_cpu]) {
          cout << "[" << NAME << "] " << __func__ << " prefetch miss";
          cout << " instr_id: " << PQ.entry[index].instr_id
               << " address: " << hex << PQ.entry[index].address;
          cout << " full_addr: " << PQ.entry[index].full_addr << dec
               << " fill_level: " << PQ.entry[index].fill_level;
          cout << " cycle: " << PQ.entry[index].event_cycle << endl;
        });

        // check mshr
        uint8_t miss_handled = 1;
        int mshr_index = check_mshr(&PQ.entry[index]);

        if (mshr_index == -2) {
          // this is a data/instruction collision in the MSHR, so we have to
          // wait before we can allocate this miss
          miss_handled = 0;
        } else if ((mshr_index == -1) &&
                   (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

          DPT(if (warmup_complete[PQ.entry[index].cpu]) {
            cout << "[" << NAME << "_PQ] " << __func__
                 << " want to add instr_id: " << PQ.entry[index].instr_id
                 << " address: " << hex << PQ.entry[index].address;
            cout << " full_addr: " << PQ.entry[index].full_addr << dec;
            cout << " occupancy: "
                 << lower_level->get_occupancy(3, PQ.entry[index].address)
                 << " SIZE: "
                 << lower_level->get_size(3, PQ.entry[index].address) << endl;
          });

          // first check if the lower level PQ is full or not
          // this is possible since multiple prefetchers can exist at each level
          // of caches
          if (lower_level) {

            if (cache_type == IS_LLC) {

              // check occupancy of DRAM
              if (lower_level->get_occupancy(1, PQ.entry[index].address) ==
                  lower_level->get_size(1, PQ.entry[index].address))
                miss_handled = 0;

              else {

                if (PQ.entry[index].is_speculative ==
                    0) { // only committed prefetch requests can invoke
                         // prefetchers of lower levels

                  // run prefetcher on prefetches from higher caches
                  if (PQ.entry[index].pf_origin_level < fill_level) {
                    PQ.entry[index].pf_metadata = llc_prefetcher_operate(
                        PQ.entry[index].address << LOG2_BLOCK_SIZE,
                        PQ.entry[index].ip, 0, PREFETCH,
                        PQ.entry[index].pf_metadata);
                  }
                }

                // add it to MSHRs if this prefetch miss will be filled to this
                // cache level
                if (PQ.entry[index].fill_level <= fill_level) {
                  add_mshr(&PQ.entry[index]);
                }

                lower_level->add_rq(&PQ.entry[index]); // add it to the DRAM RQ
              }
            }

            else {

              if (lower_level->get_occupancy(3, PQ.entry[index].address) ==
                  lower_level->get_size(3, PQ.entry[index].address))
                miss_handled = 0;

              else {
                // run prefetcher on prefetches from higher caches

                if ((PQ.entry[index].is_speculative == 0) &&
                    (PQ.entry[index].pf_origin_level !=
                     FILL_L0)) { // only committed prefetch requests can invoke
                                 // prefetchers of lower levels

                  if (PQ.entry[index].pf_origin_level < fill_level) {

                    DPT(if (warmup_complete[prefetch_cpu]) {
                      cout << "[" << NAME << "] " << __func__
                           << " lower level prefetcher_operate call; "
                              "speculative_bit: 0"
                           << endl;
                    });

#ifdef SPEC_COMMIT_L2C
                    if (cache_type == IS_L2C) {
                      handle_prefetch_l2c_call_count++;
                      PQ.entry[index].pf_metadata = l2c_prefetcher_operate(
                          PQ.entry[index].address << LOG2_BLOCK_SIZE,
                          PQ.entry[index].ip, 0, PREFETCH,
                          PQ.entry[index].pf_metadata, 0);
                    }

#endif
#ifndef SPEC_COMMIT_L2C
                    if (cache_type == IS_L2C) {
                      handle_prefetch_l2c_call_count++;
                      PQ.entry[index].pf_metadata = l2c_prefetcher_operate(
                          PQ.entry[index].address << LOG2_BLOCK_SIZE,
                          PQ.entry[index].ip, 0, PREFETCH,
                          PQ.entry[index].pf_metadata);
                    }

#endif
                  }
                }

                // add it to MSHRs if this prefetch miss will be filled to this
                // cache level
                if (PQ.entry[index].fill_level <= fill_level) {
                  add_mshr(&PQ.entry[index]);
                }

                int is_added = lower_level->add_pq(&PQ.entry[index]);
              }
            }
          }
        } else {
          if ((mshr_index == -1) &&
              (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource

            // cannot handle miss request until one of MSHRs is available
            miss_handled = 0;
            STALL[PQ.entry[index].type]++;

            if (cache_type == IS_L0I) {
              prefetch_mshr_full++;
            }

            // This case is not being considered for MSHR contention mitigation
            // because the prefetches cannot be delayed by the attacker to
            // increase the speculative window. Thus prefetch requests in PQ
            // cannot be used for pulling off an attack.
          } else if (mshr_index != -1) { // already in-flight miss

            // no need to update request except fill_level
            // update fill_level
            if (PQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
              MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;

            if ((PQ.entry[index].fill_l1i) &&
                (MSHR.entry[mshr_index].fill_l1i != 1)) {
              MSHR.entry[mshr_index].fill_l1i = 1;
            }
            if ((PQ.entry[index].fill_l1d) &&
                (MSHR.entry[mshr_index].fill_l1d != 1)) {
              MSHR.entry[mshr_index].fill_l1d = 1;
            }

            if ((PQ.entry[index].fill_l0i) &&
                (MSHR.entry[mshr_index].fill_l0i != 1)) {
              MSHR.entry[mshr_index].fill_l0i = 1;
            }
            if ((PQ.entry[index].fill_l0d) &&
                (MSHR.entry[mshr_index].fill_l0d != 1)) {
              MSHR.entry[mshr_index].fill_l0d = 1;
            }

            MSHR_MERGED[PQ.entry[index].type]++;

            DPT(if (warmup_complete[prefetch_cpu]) {
              cout << "[" << NAME << "] " << __func__ << " mshr merged";
              cout << " instr_id: " << PQ.entry[index].instr_id
                   << " prior_id: " << MSHR.entry[mshr_index].instr_id;
              cout << " address: " << hex << PQ.entry[index].address;
              cout << " full_addr: " << PQ.entry[index].full_addr << dec
                   << " fill_level: " << MSHR.entry[mshr_index].fill_level;
              cout << " cycle: " << MSHR.entry[mshr_index].event_cycle << endl;
            });
          } else { // WE SHOULD NOT REACH HERE
            cerr << "[" << NAME << "] MSHR errors" << endl;
            assert(0);
          }
        }

        if (miss_handled) {

          DPT(if (warmup_complete[prefetch_cpu]) {
            cout << "[" << NAME << "] " << __func__ << " prefetch miss handled";
            cout << " instr_id: " << PQ.entry[index].instr_id
                 << " address: " << hex << PQ.entry[index].address;
            cout << " full_addr: " << PQ.entry[index].full_addr << dec
                 << " fill_level: " << PQ.entry[index].fill_level;
            cout << " cycle: " << PQ.entry[index].event_cycle << endl;
          });

          MISS[PQ.entry[index].type]++;
          ACCESS[PQ.entry[index].type]++;

          // remove this entry from PQ
          PQ.remove_queue(&PQ.entry[index]);
          reads_available_this_cycle--;
        }
      }
    } else {
      return;
    }

    if (reads_available_this_cycle == 0) {
      return;
    }
  }
}

void CACHE::operate() {

  handle_fill();

  handle_writeback();

  reads_available_this_cycle = MAX_READ;

  handle_read();

  if (PQ.occupancy && (reads_available_this_cycle > 0)) {
    handle_prefetch();
  }
}

uint32_t CACHE::get_set(uint64_t address) {
  return (uint32_t)(address & ((1 << lg2(NUM_SET)) - 1));
}

uint32_t CACHE::get_way(uint64_t address, uint32_t set) {
  for (uint32_t way = 0; way < NUM_WAY; way++) {
    if (block[set][way].valid && (block[set][way].tag == address))
      return way;
  }

  return NUM_WAY;
}

void CACHE::fill_cache(uint32_t set, uint32_t way, PACKET *packet) {
#ifdef SANITY_CHECK
  if (cache_type == IS_ITLB) {
    if (packet->data == 0)
      assert(0);
  }

  if (cache_type == IS_DTLB) {
    if (packet->data == 0)
      assert(0);
  }

  if (cache_type == IS_STLB) {
    if (packet->data == 0)
      assert(0);
  }
#endif

  if (block[set][way].prefetch && (block[set][way].used == 0)) {
    pf_useless++;

#ifdef IPCP_PREFETCHER_NEW
    // for IPCP stats.
    if (cache_type == IS_L1D || cache_type == IS_L0D) {
      IPCP_OFFSET_USELESS_COUNT[block[set][way].ipcp_offset]++;
    }
#endif
  }

  if (block[set][way].valid == 0)
    block[set][way].valid = 1;
  block[set][way].dirty = 0;
  block[set][way].prefetch = (packet->type == PREFETCH) ? 1 : 0;

#ifdef FNLMMA
  if (cache_type == IS_L1I || cache_type == IS_L0I) {
    block[set][way].is_FNL = packet->is_FNL;
  }
#endif

#ifdef IPCP_PREFETCHER_NEW
  if (cache_type == IS_L0D || cache_type == IS_L1D) {
    block[set][way].ipcp_offset = packet->offset_value_ipcp; // for IPCP.
  }
#endif

  block[set][way].used = 0;

#ifdef IPCP_PREFETCHER
  //@Tarun: Addition for IPCP Throttling.
  block[set][way].pref_class = ((packet->pf_metadata & 0xF00) >> 8);
  block[set][way].pref_class =
      ((packet->pf_metadata & PREF_CLASS_MASK) >> NUM_OF_STRIDE_BITS);
#endif

  if (block[set][way].prefetch)
    pf_fill++;

#ifdef IPCP_PREFETCHER
  // addition for IPCP Throttling.
  if (cache_type == IS_L1D || cache_type == IS_L0D) {
    if (block[set][way].pref_class < 5) {
      pref_filled[cpu][block[set][way].pref_class]++;
    }
  }
#endif

  // set committed bit to be 0
  if (cache_type == IS_L0D || cache_type == IS_L0I) {
    block[set][way].committed = 0; // these blocks will always be speculative as
                                   // per the current implementation
  }

  block[set][way].delta = packet->delta;
  block[set][way].depth = packet->depth;
  block[set][way].signature = packet->signature;
  block[set][way].confidence = packet->confidence;

  block[set][way].tag = packet->address;
  block[set][way].address = packet->address;
  block[set][way].full_addr = packet->full_addr;
  block[set][way].data = packet->data;
  block[set][way].ip = packet->ip;
  block[set][way].cpu = packet->cpu;
  block[set][way].instr_id = packet->instr_id;

  DPT(if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "] " << __func__ << " set: " << set
         << " way: " << way;
    cout << " lru: " << block[set][way].lru << " tag: " << hex
         << block[set][way].tag << " full_addr: " << block[set][way].full_addr;
    cout << " data: " << block[set][way].data << dec << endl;
  });
}

int CACHE::check_hit(PACKET *packet) {
  uint32_t set = get_set(packet->address);
  int match_way = -1;

  if (NUM_SET < set) {
    cerr << "[" << NAME << "_ERROR] " << __func__
         << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
    cerr << " address: " << hex << packet->address
         << " full_addr: " << packet->full_addr << dec;
    cerr << " event: " << packet->event_cycle << endl;
    assert(0);
  }

  // hit
  for (uint32_t way = 0; way < NUM_WAY; way++) {
    if (block[set][way].valid && (block[set][way].tag == packet->address)) {

      match_way = way;

      DPT(if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "] " << __func__
             << " instr_id: " << packet->instr_id << " type: " << +packet->type
             << hex << " addr: " << packet->address;
        cout << " full_addr: " << packet->full_addr
             << " tag: " << block[set][way].tag
             << " data: " << block[set][way].data << dec;
        cout << " set: " << set << " way: " << way
             << " lru: " << block[set][way].lru;
        cout << " event: " << packet->event_cycle
             << " cycle: " << current_core_cycle[cpu] << endl;
      });

      break;
    }
  }

  return match_way;
}

int CACHE::invalidate_entry(uint64_t inval_addr) {
  uint32_t set = get_set(inval_addr);
  int match_way = -1;

  if (NUM_SET < set) {
    cerr << "[" << NAME << "_ERROR] " << __func__
         << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
    cerr << " inval_addr: " << hex << inval_addr << dec << endl;
    assert(0);
  }

  // invalidate
  for (uint32_t way = 0; way < NUM_WAY; way++) {
    if (block[set][way].valid && (block[set][way].tag == inval_addr)) {

      block[set][way].valid = 0;

      match_way = way;

      DPT(if (warmup_complete[cpu]) {
        cout << "[" << NAME << "] " << __func__ << " inval_addr: " << hex
             << inval_addr;
        cout << " tag: " << block[set][way].tag
             << " data: " << block[set][way].data << dec;
        cout << " set: " << set << " way: " << way
             << " lru: " << block[set][way].lru
             << " cycle: " << current_core_cycle[cpu] << endl;
      });

      break;
    }
  }

  return match_way;
}

int CACHE::add_rq(PACKET *packet) {
  // check for the latest writebacks in the write queue
  int wq_index = WQ.check_queue(packet);

  if (wq_index != -1) { // WQ has an entry of packet already.

    // check fill level
    if (packet->fill_level < fill_level) {

      packet->data = WQ.entry[wq_index].data;

      if (fill_level == FILL_L2) {
        if (packet->fill_l1i) {
          upper_level_icache[packet->cpu]->return_data(packet);
        }
        if (packet->fill_l1d) {
          upper_level_dcache[packet->cpu]->return_data(packet);
        }
      } else if (fill_level == FILL_L1) {
        if (packet->fill_l0i) {
          upper_level_icache[packet->cpu]->return_data(packet);
        }
        if (packet->fill_l0d) {
          upper_level_dcache[packet->cpu]->return_data(packet);
        }
      } else {
        if (packet->instruction)
          upper_level_icache[packet->cpu]->return_data(packet);
        if (packet->is_data)
          upper_level_dcache[packet->cpu]->return_data(packet);
      }
    }

//* ITLB, DTLB, L1I cannot have a writeback request.
#ifdef SANITY_CHECK
    if (cache_type == IS_ITLB) {
      assert(0);
    } else if (cache_type == IS_DTLB) {
      assert(0);
    }

    else if (cache_type == IS_L1I) {
      // now L1I packet can also be in the WQ because of write-through from
      // MuonTrap
    }

#endif

    // update processed packets
    if ((cache_type == IS_L0D) && (packet->type != PREFETCH)) {
      if (PROCESSED.occupancy < PROCESSED.SIZE)
        PROCESSED.add_queue(packet);

      DPT(if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "_RQ] " << __func__
             << " instr_id: " << packet->instr_id << " found recent writebacks";
        cout << hex << " read: " << packet->address
             << " writeback: " << WQ.entry[wq_index].address << dec;
        cout << " index: " << MAX_READ << " rob_signal: " << packet->rob_signal
             << endl;
      });
    }

    HIT[packet->type]++;
    ACCESS[packet->type]++;

    WQ.FORWARD++;
    RQ.ACCESS++;

    return -3;
  }

  // check for duplicates in the read queue
  int index = RQ.check_queue(packet);

  if (index != -1) { // Entry exists in the Queue.

    if (packet->instruction) {

      uint32_t rob_index = packet->rob_index;
      RQ.entry[index].rob_index_depend_on_me.insert(rob_index);
      RQ.entry[index].instruction = 1; // add as instruction type
      RQ.entry[index].instr_merged = 1;

      DPT(if (warmup_complete[packet->cpu]) {
        cout << "[INSTR_MERGED] " << __func__ << " cpu: " << packet->cpu
             << " instr_id: " << RQ.entry[index].instr_id;
        cout << " merged rob_index: " << rob_index
             << " instr_id: " << packet->instr_id << endl;
      });
    } else {
      // mark merged consumer
      if (packet->type == RFO) {
        uint32_t sq_index = packet->sq_index;
        RQ.entry[index].sq_index_depend_on_me.insert(sq_index);
        RQ.entry[index].store_merged = 1;

        DP(if (warmup_complete[packet->cpu]) {
          cout << "[DATA_MERGED SQ] " << __func__ << " cpu: " << packet->cpu
               << " instr_id: " << RQ.entry[index].instr_id;
          cout << " merged rob_index: " << packet->rob_index
               << " instr_id: " << packet->instr_id
               << " lq_index: " << packet->lq_index << endl;
        });
      } else {
        uint32_t lq_index = packet->lq_index;
        RQ.entry[index].lq_index_depend_on_me.insert(lq_index);
        RQ.entry[index].load_merged = 1;

        DP(if (warmup_complete[packet->cpu]) {
          cout << "[DATA_MERGED LQ] " << __func__ << " cpu: " << packet->cpu
               << " instr_id: " << RQ.entry[index].instr_id;
          cout << " merged rob_index: " << packet->rob_index
               << " instr_id: " << packet->instr_id
               << " lq_index: " << packet->lq_index << endl;
        });
      }
      RQ.entry[index].is_data = 1; // add as of type data
    }

    if (packet->fill_level < RQ.entry[index].fill_level)
      RQ.entry[index].fill_level = packet->fill_level;

    if ((packet->fill_l1i) && (RQ.entry[index].fill_l1i != 1)) {
      RQ.entry[index].fill_l1i = 1;
    }
    if ((packet->fill_l1d) && (RQ.entry[index].fill_l1d != 1)) {
      RQ.entry[index].fill_l1d = 1;
    }

    if ((packet->fill_l0i) && (RQ.entry[index].fill_l0i != 1)) {
      RQ.entry[index].fill_l0i = 1;
    }
    if ((packet->fill_l0d) && (RQ.entry[index].fill_l0d) != 1) {
      RQ.entry[index].fill_l0d = 1;
    }

    RQ.MERGED++;
    RQ.ACCESS++;

    return index; // merged index
  }

  // check occupancy
  if (RQ.occupancy == RQ_SIZE) {
    RQ.FULL++;
    return -2; // cannot handle this request
  }

  index = RQ.tail;

#ifdef SANITY_CHECK
  if (RQ.entry[index].address != 0) {
    cerr << "[" << NAME << "_ERROR] " << __func__
         << " is not empty index: " << index;
    cerr << " address: " << hex << RQ.entry[index].address;
    cerr << " full_addr: " << RQ.entry[index].full_addr << dec << endl;
    assert(0);
  }
#endif

  RQ.entry[index] = *packet;

  // ADD LATENCY
  if (RQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
    RQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
  else
    RQ.entry[index].event_cycle += LATENCY;

  RQ.occupancy++;
  RQ.tail++;
  if (RQ.tail >= RQ.SIZE)
    RQ.tail = 0;

  DPT(if (warmup_complete[RQ.entry[index].cpu]) {
    cout << "[" << NAME << "_RQ] " << __func__
         << " instr_id: " << RQ.entry[index].instr_id << " address: " << hex
         << RQ.entry[index].address;
    cout << " full_addr: " << RQ.entry[index].full_addr << dec;
    cout << " type: " << +RQ.entry[index].type << " head: " << RQ.head
         << " tail: " << RQ.tail << " occupancy: " << RQ.occupancy;
    cout << " event: " << RQ.entry[index].event_cycle
         << " current: " << current_core_cycle[RQ.entry[index].cpu] << endl;
  });

  if (packet->address == 0)
    assert(0);

  RQ.TO_CACHE++;
  RQ.ACCESS++;

  return -1;
}

int CACHE::add_wq(PACKET *packet) {
  // check for duplicates in the write queue
  int index = WQ.check_queue(packet);
  if (index != -1) {

    WQ.MERGED++;
    WQ.ACCESS++;

    return index; // merged index
  }

  // sanity check
  if (WQ.occupancy >= WQ.SIZE) {
    return -2;
    // assert(0);
  }

  // if there is no duplicate, add it to the write queue
  index = WQ.tail;
  if (WQ.entry[index].address != 0) {
    cerr << "[" << NAME << "_ERROR] " << __func__
         << " is not empty index: " << index;
    cerr << " address: " << hex << WQ.entry[index].address;
    cerr << " full_addr: " << WQ.entry[index].full_addr << dec << endl;
    assert(0);
  }

  WQ.entry[index] = *packet;

  // ADD LATENCY
  if (WQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
    WQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
  else
    WQ.entry[index].event_cycle += LATENCY;

  WQ.occupancy++;
  WQ.tail++;
  if (WQ.tail >= WQ.SIZE)
    WQ.tail = 0;

  DPT(if (warmup_complete[WQ.entry[index].cpu]) {
    cout << "[" << NAME << "_WQ] " << __func__
         << " instr_id: " << WQ.entry[index].instr_id << " address: " << hex
         << WQ.entry[index].address;
    cout << " full_addr: " << WQ.entry[index].full_addr << dec;
    cout << " head: " << WQ.head << " tail: " << WQ.tail
         << " occupancy: " << WQ.occupancy;
    cout << " data: " << hex << WQ.entry[index].data << dec;
    cout << " event: " << WQ.entry[index].event_cycle
         << " current: " << current_core_cycle[WQ.entry[index].cpu] << endl;
  });

  WQ.TO_CACHE++;
  WQ.ACCESS++;

  return -1;
}

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr,
                         int pf_fill_level, uint32_t prefetch_metadata,
                         uint8_t speculative_bit, uint32_t ipcp_offset_value)
#endif

#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
    int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr,
                             int pf_fill_level, uint32_t prefetch_metadata)
#endif
{

  /*
   *   Notes:
   *   1) This function is executed directly by L0D from the IPCP code itself.
   * -->       ooo_cpu[cpu].L0D.prefetch_line() 2) ipcp_offset = 1, represents a
   * speculative prefetch request. ipcp_offset = 0, represents a committed
   * prefetch request.
   */

  pf_requested++;

  if (PQ.occupancy < PQ.SIZE) { // check L0D/L1D PQ occupancy

    // this is a PIPT implementation so the distance to be added should be done
    // before this if condition.
    if ((base_addr >> LOG2_PAGE_SIZE) == (pf_addr >> LOG2_PAGE_SIZE)) {

      PACKET pf_packet;

#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
      pf_packet.is_speculative = 0;
      // Prefetch blocks will have spec_bit=0 because they'll have to filled
      // into the cache for baseline-prefetching and on-commit prefetching. The
      // spec_bit will be 1 when "SPEC_COMMIT" concept will be used for the
      // prefetching.
#endif
#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
      pf_packet.is_speculative = speculative_bit;
#endif

#ifdef IPCP_PREFETCHER_NEW
      if (cache_type == IS_L1D || cache_type == IS_L0D) {
        pf_packet.offset_value_ipcp = ipcp_offset_value;
        IPCP_OFFSET_ISSUED_COUNT[ipcp_offset_value]++;
      }
#endif

      pf_packet.fill_level = pf_fill_level;
      pf_packet.pf_origin_level = fill_level;

      if (pf_fill_level == FILL_L0) {
        pf_packet.fill_l1d = 1;
        pf_packet.fill_l0d = 1;
      }

      if (pf_fill_level == FILL_L1) {
        pf_packet.fill_l1d = 1;
        pf_packet.fill_l0d = 0;
      }

      pf_packet.pf_metadata = prefetch_metadata;
      pf_packet.cpu = cpu;
      pf_packet.instr_id = 0;
      pf_packet.address =
          pf_addr >> LOG2_BLOCK_SIZE; // HERE // 64 bit // Only block number
      pf_packet.full_addr =
          pf_addr; // Add the K dist here. // BLock number + Offset.
      uint32_t type = (prefetch_metadata & 0xF00) >> 8;
      pf_packet.ip = ip;
      pf_packet.type = PREFETCH;
      pf_packet.event_cycle = current_core_cycle[cpu];

      add_pq(&pf_packet);
      pf_issued++;

      return 1;
    }
  }

  return 0;
}

int CACHE::kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr,
                             int pf_fill_level, int delta, int depth,
                             int signature, int confidence,
                             uint32_t prefetch_metadata) {
  if (PQ.occupancy < PQ.SIZE) {
    if ((base_addr >> LOG2_PAGE_SIZE) == (pf_addr >> LOG2_PAGE_SIZE)) {

      PACKET pf_packet;
      pf_packet.fill_level = pf_fill_level;
      pf_packet.pf_origin_level = fill_level;
      if (pf_fill_level == FILL_L1) {
        pf_packet.fill_l1d = 1;
      }
      pf_packet.pf_metadata = prefetch_metadata;
      pf_packet.cpu = cpu;
      pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
      pf_packet.full_addr = pf_addr;
      pf_packet.ip = 0;
      pf_packet.type = PREFETCH;
      pf_packet.delta = delta;
      pf_packet.depth = depth;
      pf_packet.signature = signature;
      pf_packet.confidence = confidence;
      pf_packet.event_cycle = current_core_cycle[cpu];

      add_pq(&pf_packet);

      pf_issued++;

      return 1;
    }
  }

  return 0;
}

int CACHE::add_pq(PACKET *packet) {
  // check for the latest writebacks in the write queue.
  int wq_index = WQ.check_queue(packet);

  if (wq_index != -1) {

    // check fill level
    if (packet->fill_level < fill_level) {

      packet->data = WQ.entry[wq_index].data;

      if (fill_level == FILL_L2) {
        if (packet->fill_l1i) {
          upper_level_icache[packet->cpu]->return_data(packet);
        }
        if (packet->fill_l1d) {
          upper_level_dcache[packet->cpu]->return_data(packet);
        }
      } else if (fill_level == FILL_L1) {
        if (packet->fill_l0i) {
          upper_level_icache[packet->cpu]->return_data(packet);
        }
        if (packet->fill_l0d) {
          upper_level_dcache[packet->cpu]->return_data(packet);
        }
      } else {
        if (packet->instruction)
          upper_level_icache[packet->cpu]->return_data(packet);

        if (packet->is_data)
          upper_level_dcache[packet->cpu]->return_data(packet);
      }
    }

    HIT[packet->type]++;
    ACCESS[packet->type]++;

    WQ.FORWARD++;
    PQ.ACCESS++;

    return -1;
  }

  // check for duplicates in the PQ
  int index = PQ.check_queue(packet);

  if (index != -1) {
    if (packet->fill_level < PQ.entry[index].fill_level) {
      PQ.entry[index].fill_level = packet->fill_level;
    }
    if ((packet->instruction == 1) && (PQ.entry[index].instruction != 1)) {
      PQ.entry[index].instruction = 1;
    }
    if ((packet->is_data == 1) && (PQ.entry[index].is_data != 1)) {
      PQ.entry[index].is_data = 1;
    }
    if ((packet->fill_l1i) && (PQ.entry[index].fill_l1i != 1)) {
      PQ.entry[index].fill_l1i = 1;
    }
    if ((packet->fill_l1d) && (PQ.entry[index].fill_l1d != 1)) {
      PQ.entry[index].fill_l1d = 1;
    }
    if ((packet->fill_l0i) && (PQ.entry[index].fill_l0i != 1)) {
      PQ.entry[index].fill_l0i = 1;
    }
    if ((packet->fill_l0d) && (PQ.entry[index].fill_l0d != 1)) {
      PQ.entry[index].fill_l0d = 1;
    }

    PQ.MERGED++;
    PQ.ACCESS++;

    return index; // merged index
  }

  // check occupancy
  if (PQ.occupancy == PQ_SIZE) {
    PQ.FULL++;

    DPT(if (warmup_complete[packet->cpu]) {
      cout << "[" << NAME << "] cannot process add_pq since it is full" << endl;
    });

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  index = PQ.tail;

#ifdef SANITY_CHECK
  if (PQ.entry[index].address != 0) {
    cerr << "[" << NAME << "_ERROR] " << __func__
         << " is not empty index: " << index;
    cerr << " address: " << hex << PQ.entry[index].address;
    cerr << " full_addr: " << PQ.entry[index].full_addr << dec << endl;
    assert(0);
  }
#endif

  PQ.entry[index] = *packet;

  // ADD LATENCY
  if (PQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
    PQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
  else
    PQ.entry[index].event_cycle += LATENCY;

  PQ.occupancy++;
  PQ.tail++;
  if (PQ.tail >= PQ.SIZE)
    PQ.tail = 0;

  DPT(if (warmup_complete[PQ.entry[index].cpu]) {
    cout << "[" << NAME << "_PQ] " << __func__
         << " instr_id: " << PQ.entry[index].instr_id << " address: " << hex
         << PQ.entry[index].address;
    cout << " full_addr: " << PQ.entry[index].full_addr << dec;
    cout << " type: " << +PQ.entry[index].type << " head: " << PQ.head
         << " tail: " << PQ.tail << " occupancy: " << PQ.occupancy;
    cout << " event: " << PQ.entry[index].event_cycle
         << " current: " << current_core_cycle[PQ.entry[index].cpu] << endl;
  });

  if (packet->address == 0)
    assert(0);

  PQ.TO_CACHE++;
  PQ.ACCESS++;

  return -1;
}

// Keeps the data of the missed block in the corresponding MSHR entry.
void CACHE::return_data(PACKET *packet) {
  // check MSHR information
  int mshr_index = check_mshr(packet);

  // sanity check
  if (mshr_index == -1) {
    return;
  }

  // MSHR holds the most updated information about this request
  // no need to do memcpy
  MSHR.num_returned++;
  MSHR.entry[mshr_index].returned = COMPLETED;
  MSHR.entry[mshr_index].data = packet->data;
  MSHR.entry[mshr_index].pf_metadata = packet->pf_metadata;

  // this latency should not be added when bypassing the lower level caches to
  // fill L0D and L0I. ADD LATENCY
  if (packet->is_speculative == 1) {
    if (cache_type == IS_L0D || cache_type == IS_L0I || cache_type == IS_DTLB ||
        cache_type == IS_ITLB || cache_type == IS_STLB) {
      // add the latency
      if (MSHR.entry[mshr_index].event_cycle < current_core_cycle[packet->cpu])
        MSHR.entry[mshr_index].event_cycle =
            current_core_cycle[packet->cpu] + LATENCY;
      else
        MSHR.entry[mshr_index].event_cycle += LATENCY;
    }
  } else if (packet->is_speculative == 0) {
    // add the latency
    if (MSHR.entry[mshr_index].event_cycle < current_core_cycle[packet->cpu])
      MSHR.entry[mshr_index].event_cycle =
          current_core_cycle[packet->cpu] + LATENCY;
    else
      MSHR.entry[mshr_index].event_cycle += LATENCY;
  }

  update_fill_cycle();

  DPT(if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "_MSHR] " << __func__
         << " instr_id: " << MSHR.entry[mshr_index].instr_id;
    cout << " address: " << hex << MSHR.entry[mshr_index].address
         << " full_addr: " << MSHR.entry[mshr_index].full_addr;
    cout << " data: " << MSHR.entry[mshr_index].data << dec
         << " num_returned: " << MSHR.num_returned;
    cout << " index: " << mshr_index << " occupancy: " << MSHR.occupancy;
    cout << " event: " << MSHR.entry[mshr_index].event_cycle
         << " current: " << current_core_cycle[packet->cpu]
         << " next: " << MSHR.next_fill_cycle << endl;
  });
}

// Updates the MSHR next_fill_index and next_fill_cycle value.
void CACHE::update_fill_cycle() {
  // update next_fill_cycle
  uint64_t min_cycle = UINT64_MAX;
  uint32_t min_index = MSHR.SIZE;
  for (uint32_t i = 0; i < MSHR.SIZE; i++) {
    if ((MSHR.entry[i].returned == COMPLETED) &&
        (MSHR.entry[i].event_cycle < min_cycle)) {
      min_cycle = MSHR.entry[i].event_cycle;
      min_index = i;
    }

    DPT(if (warmup_complete[MSHR.entry[i].cpu]) {
      cout << "[" << NAME << "_MSHR] " << __func__
           << " checking instr_id: " << MSHR.entry[i].instr_id;
      cout << " address: " << hex << MSHR.entry[i].address
           << " full_addr: " << MSHR.entry[i].full_addr;
      cout << " data: " << MSHR.entry[i].data << dec
           << " returned: " << +MSHR.entry[i].returned
           << " fill_level: " << MSHR.entry[i].fill_level;
      cout << " index: " << i << " occupancy: " << MSHR.occupancy;
      cout << " event: " << MSHR.entry[i].event_cycle
           << " current: " << current_core_cycle[MSHR.entry[i].cpu]
           << " next: " << MSHR.next_fill_cycle << endl;
    });
  }

  MSHR.next_fill_cycle = min_cycle;
  MSHR.next_fill_index = min_index;
  if (min_index < MSHR.SIZE) {

    DPT(if (warmup_complete[MSHR.entry[min_index].cpu]) {
      cout << "[" << NAME << "_MSHR] " << __func__
           << " instr_id: " << MSHR.entry[min_index].instr_id;
      cout << " address: " << hex << MSHR.entry[min_index].address
           << " full_addr: " << MSHR.entry[min_index].full_addr;
      cout << " data: " << MSHR.entry[min_index].data << dec
           << " num_returned: " << MSHR.num_returned;
      cout << " event: " << MSHR.entry[min_index].event_cycle
           << " current: " << current_core_cycle[MSHR.entry[min_index].cpu]
           << " next: " << MSHR.next_fill_cycle << endl;
    });
  }
}

// If found in MSHR then returns the index, else returns -1.
int CACHE::check_mshr(PACKET *packet) {
  // search mshr

  for (uint32_t index = 0; index < MSHR_SIZE; index++) {
    if (MSHR.entry[index].address == packet->address) {
      DPT(if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "_MSHR] " << __func__
             << " same entry instr_id: " << packet->instr_id
             << " prior_id: " << MSHR.entry[index].instr_id;
        cout << " address: " << hex << packet->address;
        cout << " full_addr: " << packet->full_addr << dec << endl;
      });

      return index;
    }
  }

  DPT(if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "_MSHR] " << __func__ << " new address: " << hex
         << packet->address;
    cout << " full_addr: " << packet->full_addr << dec << endl;
  });

  DPT(if (warmup_complete[packet->cpu] && (MSHR.occupancy == MSHR_SIZE)) {
    cout << "[" << NAME << "_MSHR] " << __func__ << " mshr is full";
    cout << " instr_id: " << packet->instr_id
         << " mshr occupancy: " << MSHR.occupancy;
    cout << " address: " << hex << packet->address;
    cout << " full_addr: " << packet->full_addr << dec;
    cout << " cycle: " << current_core_cycle[packet->cpu] << endl;
  });

  return -1;
}

void CACHE::add_mshr(PACKET *packet) {
  uint32_t index = 0;

  packet->cycle_enqueued = current_core_cycle[packet->cpu];

  // search mshr
  for (index = 0; index < MSHR_SIZE; index++) {
    if (MSHR.entry[index].address == 0) {

      MSHR.entry[index] = *packet;
      MSHR.entry[index].returned = INFLIGHT;
      MSHR.occupancy++;

      DPT(if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "_MSHR] " << __func__
             << " instr_id: " << packet->instr_id;
        cout << " address: " << hex << packet->address
             << " full_addr: " << packet->full_addr << dec;
        cout << " index: " << index << " occupancy: " << MSHR.occupancy << endl;
      });

      break;
    }
  }
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address) {
  if (queue_type == 0)
    return MSHR.occupancy;
  else if (queue_type == 1)
    return RQ.occupancy;
  else if (queue_type == 2)
    return WQ.occupancy;
  else if (queue_type == 3)
    return PQ.occupancy;

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address) {
  if (queue_type == 0)
    return MSHR.SIZE;
  else if (queue_type == 1)
    return RQ.SIZE;
  else if (queue_type == 2)
    return WQ.SIZE;
  else if (queue_type == 3)
    return PQ.SIZE;

  return 0;
}

void CACHE::increment_WQ_FULL(uint64_t address) { WQ.FULL++; }