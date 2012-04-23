#ifndef MCP_TEST_MEMTESTBINSMGR_HEADER
#define MCP_TEST_MEMTESTBINSMGR_HEADER

#include <inttypes.h>
#include "thread_barrier.hpp"

namespace test {

// A scalable-memory-allocator performance (speed-only here) test class
//
#define RANDOM(s) (rng() % (s))
#define REALLOC_MAX 2000
#define ACTIONS_MAX 30
#ifndef TEST
#define TEST 0
#endif

using base::Barrier;

class MemTestBinsMgr {
public:
  MemTestBinsMgr(size_t perbinsize, size_t numbins, uint32_t imax,
      int seed, Barrier* barrier)
    : maxperbin_size_(perbinsize), num_bins_(numbins), imax_(imax),
      rnd_seed_((uint64_t)((imax_ * maxperbin_size_ + seed) ^ num_bins_)),
      binsArr_(new Bin[num_bins_]), barrier_(barrier) { }
  ~MemTestBinsMgr() { delete [] binsArr_; }
  void MallocTest();

private:
  struct Bin {
    unsigned char* ptr;
    size_t binsize;
    Bin() : ptr(NULL), binsize(0) { }
  };
  const size_t   maxperbin_size_;  // Maximum per bin mem-alloc-size
  const size_t   num_bins_;        // Total # of bins
  const uint32_t imax_;            // # of operations for second round
  uint64_t       rnd_seed_;        // Thread-local var, random-num-seed
  Bin*           binsArr_;
  Barrier*       barrier_;

  // Private test-methods
  void BinAlloc(Bin* abin, size_t allocsize, uint32_t randnum);
  void BinFree(Bin* abin);
  uint32_t rng(void);

  // Non-copyable, non-assignable
  MemTestBinsMgr(MemTestBinsMgr&);
  MemTestBinsMgr& operator=(MemTestBinsMgr&);
};

}  // namespace test

#endif  // MCP_TEST_MEMTESTBINSMGR_HEADER
