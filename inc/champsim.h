#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <string>

#define SPECULATIVE_BIT 1 // speculative bit for the instructions
#define L0I_CACHE
#define L0D_CACHE
#define L0I_SIZE 2
#define L0D_SIZE 2

//*----------- Modifiable #defines -------------------
#define SINGLECORE
// #define MULTICORE

// #define BASELINE_FNLMMA
#define BASELINE_IPCP

// #define FNLMMA_INSECURE
// #define FNLMMA_MUONTRAP
// #define FNLMMA_SPECPREF

// #define IPCP_INSECURE
#define IPCP_MUONTRAP
// #define IPCP_SPECPREF
// #define IPCP_GHOST_LOADS
//*---------------------------------------------------

#ifdef BASELINE_FNLMMA
#ifdef MULTICORE
#define MULTICORE_BASELINE
#endif
#endif

#ifdef BASELINE_IPCP
#define SPEC_INST_CLASSIFICATOIN // for speculative instruction classification
// #define NON_SPEC_10TH_IFETCH // for non-speculative instruction fetches
#define NON_SPEC_10TH_LOAD // for non-speculative loads
#ifdef SINGLECORE
#define NS_INST_PRIORITY // (Off with FNLMMA) (On with IPCP) Gives higher
                         // priority to non-speculative instructions. Turned off
                         // with FNL+MMA because all the instructions are
                         // speculative since fetch width is 5 and every 5th
                         // instruction is a branch.
#endif
#ifdef MULTICORE
#define MULTICORE_BASELINE
#endif
#endif

// * <----------  L1I ------------>
#ifdef FNLMMA_INSECURE
#endif

#ifdef FNLMMA_MUONTRAP
#define MUONTRAP_PREFETCH_ON_COMMIT // Implements the On-Commit implementation
#define PREFETCH_ON_COMMIT_L1I
#endif

#ifdef FNLMMA_SPECPREF
#define MUONTRAP_PREFETCH_ON_COMMIT // Implements the On-Commit implementation
#define SPEC_COMMIT_L1I             //-> SpecPref
#define DISTAHEAD                                                              \
  16 // (10 with NP/POC) (16 with SpecPref) can be tested for more. but 25 was
     // degrading
#define MAXFNL 5  // (best as per simls)
#define AHEADPRED // For allowing MMA execution
#define FNLMMA    // -> for fnl and mma (useful,useless) counters
#define SPEC_MAX_CALLS                                                         \
  5 // -> Number of prefetch issues allowed during speculative prefetching
#define SPECFITERFNLON // (OFF Always) filter for speculative FNL prefetches
#endif

#ifdef IPCP_INSECURE
#define NON_SPEC_10TH_IFETCH
#define NON_SPEC_10TH_LOAD
#define IPCP_PREFETCHER
#ifdef SINGLECORE
#define NS_INST_PRIORITY // Allows the non-speculative instructions to have a
                         // higher priority over speculative instructions.
#endif
#define GLOBAL_STREAM_DEGREE 6
#define CONSTANT_STRIDE_DEGREE 3
#define COMPLEX_STRIDE_DEGREE 3
#define NEXT_LINE 1
#endif

#ifdef IPCP_MUONTRAP
#define NON_SPEC_10TH_IFETCH
#define NON_SPEC_10TH_LOAD
#define MUONTRAP_PREFETCH_ON_COMMIT
#define PREFETCH_ON_COMMIT_L1D
#define IPCP_PREFETCHER
#ifdef SINGLECORE
#define NS_INST_PRIORITY // Allows the non-speculative instructions to have a
                         // higher priority over speculative instructions.
#endif
#define GLOBAL_STREAM_DEGREE 6
#define CONSTANT_STRIDE_DEGREE 3
#define COMPLEX_STRIDE_DEGREE 3
#define NEXT_LINE 1
#endif

#ifdef IPCP_SPECPREF
#define NON_SPEC_10TH_IFETCH
#define NON_SPEC_10TH_LOAD
#define MUONTRAP_PREFETCH_ON_COMMIT
#define SPEC_COMMIT_L1D // SpecPref
#define IPCP_PREFETCHER
#ifdef SINGLECORE
#define NS_INST_PRIORITY
#endif

#define GLOBAL_STREAM_DEGREE 16
#define CONSTANT_STRIDE_DEGREE 16
#define COMPLEX_STRIDE_DEGREE 16
#define NEXT_LINE 3 //-> Increasing this does not help anyhow

#define IPCP_PREFETCHER_NEW
#define SPEC_FILTER_ON //-> Keep IPCP filter updation ON during speculative
                       // prefetching
#define SPEC_MAX_CALLS                                                         \
  1 //-> Represents the maximum number of prefetches that can be issued
    // speculatively. //Note: 0 SPEC_MAX_CALLS -> No prefetching speculatively
#define REDUCED_SPEC_PREFETCHNG //-> Reduces the speculative prefetching amount

// -> During speculative prefetching one on every REDUCE_LIMIT prefetcher
// invocations issue prefetches.
#if L0D_SIZE == 2
#define REDUCE_LIMIT 700
#endif

#if L0D_SIZE == 4
#define REDUCE_LIMIT 500
#endif

#if L0D_SIZE == 8
#define REDUCE_LIMIT 300
#endif

#if L0D_SIZE == 16
#define REDUCE_LIMIT 50
#endif

#if L0D_SIZE == 32
#define REDUCE_LIMIT 50
#endif
#endif

#ifdef IPCP_GHOST_LOADS
#define NON_SPEC_10TH_IFETCH
#define NON_SPEC_10TH_LOAD
#define MUONTRAP_PREFETCH_ON_COMMIT
#define GHOST_PREFETCHING
#define IPCP_PREFETCHER
#ifdef SINGLECORE
#define NS_INST_PRIORITY
#endif
#define GLOBAL_STREAM_DEGREE 6
#define CONSTANT_STRIDE_DEGREE 3
#define COMPLEX_STRIDE_DEGREE 3
#define NEXT_LINE 1
#define IPCP_PREFETCHER_NEW
#define SPEC_FILTER_ON
#endif

#define NUM_OFFSETS 2

extern uint8_t speculative_prefetch;

// *--------------------------------------------

// USEFUL MACROS
#define SANITY_CHECK
// #define LLC_BYPASS
#define DRC_BYPASS
#define NO_CRC2_COMPILE

// #define DEBUG_PRINT
#ifdef DEBUG_PRINT
#define DP(x) x
#else
#define DP(x)
#endif

// #define DEBUG_PRINT_TARUN
#ifdef DEBUG_PRINT_TARUN
#define DPT(x) x
#else
#define DPT(x)
#endif

// CPU
#define NUM_CPUS 1
#define CPU_FREQ 4000
#define DRAM_IO_FREQ 3200
#define PAGE_SIZE 4096
#define LOG2_PAGE_SIZE 12

// CACHE
#define BLOCK_SIZE 64
#define LOG2_BLOCK_SIZE 6
#define MAX_READ_PER_CYCLE 8
#define MAX_FILL_PER_CYCLE 1

#define INFLIGHT 1
#define COMPLETED 2

#define FILL_L0 1
#define FILL_L1 2
#define FILL_L2 4
#define FILL_LLC 8
#define FILL_DRC 16
#define FILL_DRAM 32

// DRAM
#ifndef MULTICORE_BASELINE
#if NUM_CPUS == 8
#define DRAM_CHANNELS                                                          \
  2 // default: assuming one DIMM per one channel 4GB * 1 => 4GB off-chip memory
#define LOG2_DRAM_CHANNELS 1
#else
#define DRAM_CHANNELS                                                          \
  1 // default: assuming one DIMM per one channel 4GB * 1 => 4GB off-chip memory
#define LOG2_DRAM_CHANNELS 0
#endif
#endif

#ifdef MULTICORE_BASELINE
#define DRAM_CHANNELS                                                          \
  2 // default: assuming one DIMM per one channel 4GB * 1 => 4GB off-chip memory
#define LOG2_DRAM_CHANNELS 1
#endif

#define DRAM_RANKS 1 // 512MB * 8 ranks => 4GB per DIMM
#define LOG2_DRAM_RANKS 0
#define DRAM_BANKS 8 // 64MB * 8 banks => 512MB per rank
#define LOG2_DRAM_BANKS 3
#define DRAM_ROWS 65536 // 2KB * 32K rows => 64MB per bank
#define LOG2_DRAM_ROWS 16
#define DRAM_COLUMNS                                                           \
  128 // 64B * 32 column chunks (Assuming 1B DRAM cell * 8 chips * 8
      // transactions = 64B size of column chunks) => 2KB per row
#define LOG2_DRAM_COLUMNS 7
#define DRAM_ROW_SIZE (BLOCK_SIZE * DRAM_COLUMNS / 1024)

#define DRAM_SIZE                                                              \
  (DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_ROW_SIZE / 1024)
#define DRAM_PAGES ((DRAM_SIZE << 10) >> 2)
//#define DRAM_PAGES 10

using namespace std;

extern uint8_t warmup_complete[NUM_CPUS], simulation_complete[NUM_CPUS],
    all_warmup_complete, all_simulation_complete, MAX_INSTR_DESTINATIONS,
    knob_cloudsuite, knob_low_bandwidth;

extern uint64_t current_core_cycle[NUM_CPUS], stall_cycle[NUM_CPUS],
    branch_resolution_timer[NUM_CPUS], //@Sumon: for speculative instruction
                                       // classification
    last_drc_read_mode, last_drc_write_mode, drc_blocks;

extern queue<uint64_t> page_queue;
extern map<uint64_t, uint64_t> page_table, inverse_table, recent_page,
    unique_cl[NUM_CPUS];
extern uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS],
    allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS],
    major_fault[NUM_CPUS];

void print_stats();
uint64_t rotl64(uint64_t n, unsigned int c), rotr64(uint64_t n, unsigned int c),
    va_to_pa(uint32_t cpu, uint64_t instr_id, uint64_t va,
             uint64_t unique_vpage, uint8_t is_code);

// log base 2 function from efectiu
int lg2(int n);

// smart random number generator
class RANDOM {
public:
  std::random_device rd;
  std::mt19937_64 engine{rd()};
  std::uniform_int_distribution<uint64_t> dist{
      0, 0xFFFFFFFFF}; // used to generate random physical page numbers

  RANDOM(uint64_t seed) { engine.seed(seed); }

  uint64_t draw_rand() { return dist(engine); };
};
extern uint64_t champsim_seed;
#endif
