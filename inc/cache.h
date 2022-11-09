#ifndef CACHE_H
#define CACHE_H
#include "memory_class.h"
using namespace std;
//<-------------------------------------------------My #defines
//-------------------------------------------------------->
#include <set>

//* Generic variables ========================
extern uint64_t mshr_full_l0i, mshr_full_l0d, mshr_full_l1i, mshr_full_l1d,
    mshr_full_l2c, mshr_full_llc, prefetched_block_hit_mshr_buffer;

//* FNL+MMA  ---------------
extern uint64_t non_spec_fnlmma_call, spec_fnlmma_call;

//* IPCP -------------------------
extern uint64_t l0d_accesses, interval_l1d_misses, useful_l1d, useful_both,
    l1d_wq_full;

//----------------------------------

//* Defined Variables ----------------
extern uint64_t total_prefetches, times_prefetched;

extern uint64_t last_core_cycles, epoch;

extern uint64_t prefetch_hits_l1d, demand_misses, load_accesses;

//* L2C -------------------------
extern uint64_t prefetch_hits_l2c, prefetch_hits_l2c_RQ, l2c_handle_read;
//*--------------------------------------------------------------

// PAGE
extern uint32_t PAGE_TABLE_LATENCY, SWAP_LATENCY;

// Count Variables
extern uint64_t l1d_call_count, prev_instr_id, curr_inst_id, o3_count,
    other_count, l1d_map_size, l1d_max_map_size, handle_prefetch_l1d_call_count,
    handle_prefetch_l2c_call_count, same_block_addr_count,
    same_index_same_block, access_count, IPCP_OFFSET_USEFUL_COUNT[NUM_OFFSETS],
    IPCP_OFFSET_USELESS_COUNT[NUM_OFFSETS],
    IPCP_OFFSET_ISSUED_COUNT[NUM_OFFSETS], l1i_map_size, l1i_max_map_size,
    l1d_hits, l1d_miss, l1d_access;

// CACHE TYPE
#define IS_ITLB 0
#define IS_DTLB 1
#define IS_STLB 2
#define IS_L0I 3 // L0I_CACHE
#define IS_L0D 4 // L0D_CACHE
#define IS_L1I 5
#define IS_L1D 6
#define IS_L2C 7
#define IS_LLC 8

// INSTRUCTION TLB
#define ITLB_SET 16
#define ITLB_WAY 4
#define ITLB_RQ_SIZE 16
#define ITLB_WQ_SIZE 16
#define ITLB_PQ_SIZE 0
#define ITLB_MSHR_SIZE 8
#define ITLB_LATENCY 1

// DATA TLB
#define DTLB_SET 16
#define DTLB_WAY 4
#define DTLB_RQ_SIZE 16
#define DTLB_WQ_SIZE 16
#define DTLB_PQ_SIZE 0
#define DTLB_MSHR_SIZE 8
#define DTLB_LATENCY 1

// SECOND LEVEL TLB
#define STLB_SET 128
#define STLB_WAY 12
#define STLB_RQ_SIZE 32
#define STLB_WQ_SIZE 32
#define STLB_PQ_SIZE 0
#define STLB_MSHR_SIZE 16
#define STLB_LATENCY 8

#ifdef NS_INST_PRIORITY
#define MSHR_SPEC_BUFFER_SIZE 4
#endif

// L0 DATA CACHE                                // <-- L0D MuonTrap

#if L0D_SIZE == 1 // <-- For Ghost Buffer
#define L0D_SET 4
#define L0D_WAY 2
#define L0D_LATENCY 1
#endif

#if L0D_SIZE == 2
#define L0D_SET 8
#define L0D_WAY 4
#define L0D_LATENCY 1
#endif

#if L0D_SIZE == 4
#define L0D_SET 16
#define L0D_WAY 4
#define L0D_LATENCY 1
#endif

#if L0D_SIZE == 8
#define L0D_SET 32
#define L0D_WAY 4
#define L0D_LATENCY 2
#endif

#if L0D_SIZE == 16
#define L0D_SET 32
#define L0D_WAY 8
#define L0D_LATENCY 3
#endif

#if L0D_SIZE == 32
#define L0D_SET 64
#define L0D_WAY 8
#define L0D_LATENCY 4
#endif

#define L0D_RQ_SIZE 64
#define L0D_WQ_SIZE 64
#define L0D_PQ_SIZE 16
#define L0D_MSHR_SIZE 16

// L0 INSTRUCTION CACHE
#if L0I_SIZE == 2
#define L0I_SET 8
#define L0I_WAY 4
#define L0I_LATENCY 1
#endif

#if L0I_SIZE == 4
#define L0I_SET 16
#define L0I_WAY 4
#define L0I_LATENCY 1
#endif

#if L0I_SIZE == 8
#define L0I_SET 32
#define L0I_WAY 4
#define L0I_LATENCY 2
#endif

#if L0I_SIZE == 16
#define L0I_SET 32
#define L0I_WAY 8
#define L0I_LATENCY 3
#endif

#if L0I_SIZE == 32
#define L0I_SET 64
#define L0I_WAY 8
#define L0I_LATENCY 4
#endif

#define L0I_RQ_SIZE 64
#define L0I_WQ_SIZE 64
#define L0I_PQ_SIZE 16
#define L0I_MSHR_SIZE 8

// L1 INSTRUCTION CACHE
#define L1I_SET 64
#define L1I_WAY 8
#define L1I_MSHR_SIZE 8
#define L1I_RQ_SIZE 64
#define L1I_WQ_SIZE 64
#define L1I_PQ_SIZE 16
#define L1I_LATENCY 4

// L1 DATA CACHE
#define L1D_SET 64
#define L1D_WAY 12
#define L1D_RQ_SIZE 64
#define L1D_WQ_SIZE 64
#define L1D_PQ_SIZE 16 // <-- L1D PQ size
#define L1D_MSHR_SIZE 16
#define L1D_LATENCY 5

// L2 CACHE
#define L2C_SET 1024
#define L2C_WAY 8
#define L2C_RQ_SIZE 32
#define L2C_WQ_SIZE 32
#define L2C_PQ_SIZE 16
#define L2C_MSHR_SIZE 32
#define L2C_LATENCY 10 // 4/5 (L1I or L1D) + 10 = 14/15 cycles

// LAST LEVEL CACHE
#ifndef MULTICORE_BASELINE
#define LLC_SET NUM_CPUS * 2048
#define LLC_WAY 16
#define LLC_RQ_SIZE NUM_CPUS *L2C_MSHR_SIZE // 48
#define LLC_WQ_SIZE NUM_CPUS *L2C_MSHR_SIZE // 48
#define LLC_PQ_SIZE NUM_CPUS * 32
#define LLC_MSHR_SIZE NUM_CPUS * 64
#define LLC_LATENCY 20 // 4/5 (L1I or L1D) + 10 + 20 = 34/35 cycles
#endif

#ifdef MULTICORE_BASELINE
#define LLC_SET 8 * 2048
#define LLC_WAY 16
#define LLC_RQ_SIZE 8 * L2C_MSHR_SIZE
#define LLC_WQ_SIZE 8 * L2C_MSHR_SIZE
#define LLC_PQ_SIZE 8 * 32
#define LLC_MSHR_SIZE 8 * 64
#define LLC_LATENCY 20 // 4/5 (L1I or L1D) + 10 + 20 = 34/35 cycles
#endif

class CACHE : public MEMORY {
public:
#ifdef IPCP_PREFETCHER
  uint64_t pref_useful[NUM_CPUS][6], pref_filled[NUM_CPUS][6],
      pref_late[NUM_CPUS][6];
#endif

  uint64_t lateness_counter, lateness_FNL_counter, lateness_MMA_counter,
      lateness_Temporal_counter, pf_useful_2, pf_FNL_useful, pf_MMA_useful,
      pf_Temporal_useful, not_defined_useful;

#ifdef NS_INST_PRIORITY
  uint64_t mshr_spec_buffer_full;
#endif

  uint32_t cpu;
  const string NAME;
  const uint32_t NUM_SET, NUM_WAY, NUM_LINE, WQ_SIZE, RQ_SIZE, PQ_SIZE,
      MSHR_SIZE;
  uint32_t LATENCY;
  BLOCK **block;
  int fill_level;
  uint32_t MAX_READ, MAX_FILL;
  uint32_t reads_available_this_cycle;
  uint8_t cache_type;

  // prefetch stats
  uint64_t pf_requested, pf_issued, pf_useful, pf_useless, pf_fill;

  // queues
  PACKET_QUEUE WQ{NAME + "_WQ", WQ_SIZE},       // write queue
      RQ{NAME + "_RQ", RQ_SIZE},                // read queue
      PQ{NAME + "_PQ", PQ_SIZE},                // prefetch queue
      MSHR{NAME + "_MSHR", MSHR_SIZE},          // MSHR
      PROCESSED{NAME + "_PROCESSED", ROB_SIZE}; // processed queue

#ifdef NS_INST_PRIORITY
  PACKET_QUEUE MSHR_SPEC_BUFFER{
      NAME + "_MSHR_SPEC_BUFFER",
      MSHR_SPEC_BUFFER_SIZE}; // Used for storing the speculative requests from
                              // MSHR
#endif

  uint64_t sim_access[NUM_CPUS][NUM_TYPES], sim_hit[NUM_CPUS][NUM_TYPES],
      sim_miss[NUM_CPUS][NUM_TYPES], roi_access[NUM_CPUS][NUM_TYPES],
      roi_hit[NUM_CPUS][NUM_TYPES], roi_miss[NUM_CPUS][NUM_TYPES];

  uint64_t total_miss_latency;

  // constructor
  CACHE(string v1, uint32_t v2, int v3, uint32_t v4, uint32_t v5, uint32_t v6,
        uint32_t v7, uint32_t v8)
      : NAME(v1), NUM_SET(v2), NUM_WAY(v3), NUM_LINE(v4), WQ_SIZE(v5),
        RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8) {

    LATENCY = 0;

    // cache block
    block = new BLOCK *[NUM_SET];
    for (uint32_t i = 0; i < NUM_SET; i++) {
      block[i] = new BLOCK[NUM_WAY];

      for (uint32_t j = 0; j < NUM_WAY; j++) {
        block[i][j].lru = j;
      }
    }

    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      upper_level_icache[i] = NULL;
      upper_level_dcache[i] = NULL;

      for (uint32_t j = 0; j < NUM_TYPES; j++) {
        sim_access[i][j] = 0;
        sim_hit[i][j] = 0;
        sim_miss[i][j] = 0;
        roi_access[i][j] = 0;
        roi_hit[i][j] = 0;
        roi_miss[i][j] = 0;
      }
    }

    total_miss_latency = 0;

    lower_level = NULL;
    extra_interface = NULL;
    fill_level = -1;
    MAX_READ = 1;
    MAX_FILL = 1;

    pf_requested = 0;
    pf_issued = 0;
    pf_useful = 0;
    pf_useless = 0;
    pf_fill = 0;

    pf_useful_2 = 0;
    pf_FNL_useful = 0;
    pf_MMA_useful = 0, not_defined_useful = 0;
    lateness_counter = 0;
    lateness_FNL_counter = 0;
    lateness_MMA_counter = 0;

#ifdef NS_INST_PRIORITY
    mshr_spec_buffer_full = 0;
#endif
  };

  // destructor
  ~CACHE() {
    for (uint32_t i = 0; i < NUM_SET; i++)
      delete[] block[i];
    delete[] block;
  };

  // functions
  int add_rq(PACKET *packet), add_wq(PACKET *packet), add_pq(PACKET *packet);

  void return_data(PACKET *packet), operate(),
      increment_WQ_FULL(uint64_t address);

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
      get_size(uint8_t queue_type, uint64_t address);

  int check_hit(PACKET *packet), invalidate_entry(uint64_t inval_addr),
      check_mshr(PACKET *packet);
#ifdef IPCP_PREFETCHER_NEW
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr,
                    int prefetch_fill_level, uint32_t prefetch_metadata,
                    uint8_t speculative_bit, uint32_t ipcp_offset_value);
#endif
#ifndef IPCP_PREFETCHER_NEW
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr,
                    int prefetch_fill_level, uint32_t prefetch_metadata);
#endif
  int kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr,
                        int prefetch_fill_level, int delta, int depth,
                        int signature, int confidence,
                        uint32_t prefetch_metadata);

  void handle_fill(), handle_writeback(), handle_read(), handle_prefetch();

  void add_mshr(PACKET *packet), update_fill_cycle(),
      llc_initialize_replacement(),
      update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way,
                               uint64_t full_addr, uint64_t ip,
                               uint64_t victim_addr, uint32_t type,
                               uint8_t hit),
      llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way,
                                   uint64_t full_addr, uint64_t ip,
                                   uint64_t victim_addr, uint32_t type,
                                   uint8_t hit),
      lru_update(uint32_t set, uint32_t way),
      fill_cache(uint32_t set, uint32_t way, PACKET *packet),
      replacement_final_stats(), llc_replacement_final_stats(),
      // prefetcher_initialize(),
      l1d_prefetcher_initialize(), l2c_prefetcher_initialize(),
      llc_prefetcher_initialize(),

#if defined(SPEC_COMMIT_L1D) || defined(GHOST_PREFETCHING)
      prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                         uint8_t type, uint8_t speculative_bit),
      l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                             uint8_t type, uint8_t speculative_bit),
#endif
#if !defined(SPEC_COMMIT_L1D) && !defined(GHOST_PREFETCHING)
      prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                         uint8_t type),
      l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                             uint8_t type),
#endif

      prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way,
                            uint8_t prefetch, uint64_t evicted_addr),
      l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way,
                                uint8_t prefetch, uint64_t evicted_addr,
                                uint32_t metadata_in),

      // prefetcher_final_stats(),
      l1d_prefetcher_final_stats(), l2c_prefetcher_final_stats(),
      llc_prefetcher_final_stats();

//* @Tarun: Changing the declaration of cache_operate to have a speculative bit
// at the end.
#ifdef SPEC_COMMIT_L1I
  void (*l1i_prefetcher_cache_operate)(uint32_t, uint64_t, uint8_t, uint8_t,
                                       uint8_t);
#endif

#ifndef SPEC_COMMIT_L1I
  void (*l1i_prefetcher_cache_operate)(uint32_t, uint64_t, uint8_t, uint8_t);
#endif

  void (*l1i_prefetcher_cache_fill)(uint32_t, uint64_t, uint32_t, uint32_t,
                                    uint8_t, uint64_t);

#ifdef FNLMMA
  void l1i_prefetcher_warmup_reset();
#endif

#ifdef SPEC_COMMIT_L2C
  uint32_t l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                                  uint8_t type, uint32_t metadata_in,
                                  uint8_t speculative_bit);
#endif
#ifndef SPEC_COMMIT_L2C
  uint32_t l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                                  uint8_t type, uint32_t metadata_in);
#endif

  uint32_t llc_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit,
                                  uint8_t type, uint32_t metadata_in),
      l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way,
                                uint8_t prefetch, uint64_t evicted_addr,
                                uint32_t metadata_in),
      llc_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way,
                                uint8_t prefetch, uint64_t evicted_addr,
                                uint32_t metadata_in);

  uint32_t get_set(uint64_t address), get_way(uint64_t address, uint32_t set),
      find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set,
                  const BLOCK *current_set, uint64_t ip, uint64_t full_addr,
                  uint32_t type),
      llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set,
                      const BLOCK *current_set, uint64_t ip, uint64_t full_addr,
                      uint32_t type),
      lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set,
                 const BLOCK *current_set, uint64_t ip, uint64_t full_addr,
                 uint32_t type);
};

#endif
