#ifndef MCP_TEST_MEMTESTBINSMGR
#define MCP_TEST_MEMTESTBINSMGR

#include <cstdio>
#include <cstdlib>
#include <malloc.h>

#include "memtest_binsmgr.hpp"
#include "logging.hpp"

namespace test {

// Other memory-checking functions
static void mem_init(unsigned char *ptr, size_t size);
static int mem_check(unsigned char *ptr, size_t size);
static int zero_check(void *p, size_t size);

// A scalable-memory-allocator performance (speed-only here) test

void MemTestBinsMgr::BinAlloc(Bin* abin, size_t allocsize,
    uint32_t randnum) {
#if TEST > 0
  if (mem_check(abin->ptr, abin->binsize)) {
    LOG(LogMessage::ERROR) << "Memory Corrupt!";
    exit(1);
  }
#endif
  randnum %= 1024;

  if (randnum < 4) {  /* memalign */
    if (abin->binsize > 0)
      free(abin->ptr);
    abin->ptr = (unsigned char*) memalign(sizeof(uint32_t) << randnum,
        allocsize);
  } else if (randnum < 20) {  /* calloc */
    if (abin->binsize > 0)
      free(abin->ptr);
    abin->ptr = (unsigned char*) calloc(allocsize, 1);
#if TEST > 0
    if (zero_check(abin->ptr, allocsize)) {
      size_t i;
      for (i = 0; i < allocsize; i++) {
        if (abin->ptr[i])
          break;
      }
      LOG(LogMessage::ERROR) << "calloc'ed memory non-zero: "
        << "( ptr= " << abin->ptr << " , i= " << i << " )";
      exit(1);
    }
#endif
  } else if ((randnum < 100) && (abin->binsize < REALLOC_MAX)) {
    /* realloc */
    if (!abin->binsize)
      abin->ptr = NULL;
    abin->ptr = (unsigned char*) realloc(abin->ptr, allocsize);
  } else {  /* malloc */
    if (abin->binsize > 0)
      free(abin->ptr);
    abin->ptr = (unsigned char*) malloc(allocsize);
  }

  if (!abin->ptr) {
    LOG(LogMessage::ERROR) << "Out of memory (r=" << randnum <<
      ", size=" << (unsigned long)allocsize << ")!\n";
    exit(1);
  }

  abin->binsize = allocsize;
#if TEST > 0
  mem_init(abin->ptr, abin->binsize);
#endif
}

void MemTestBinsMgr::BinFree(Bin* abin) {
  if (!abin->binsize)  // This bin is empty
    return;

#if TEST > 0
  if (mem_check(abin->ptr, abin->binsize)) {
    LOG(LogMessage::ERROR) << "Memory Corrupt!";
    exit(1);
  }
#endif

  free(abin->ptr);
  abin->binsize = 0;
}

// Ultra-fast random-number-generator: Use a fast hash of integers.
// 2**64 Period. Passes Diehard and TestU01 at maximum settings
inline uint32_t MemTestBinsMgr::rng(void) {
  uint64_t c = 7319936632422683443ULL;
  uint64_t x = (rnd_seed_ += c);

  x ^= x >> 32;
  x *= c;
  x ^= x >> 32;
  x *= c;
  x ^= x >> 32;

  /* Return lower 32bits */
  return static_cast<uint32_t>(x);
}

void MemTestBinsMgr::MallocTest() {
  barrier_->wait();  // Barrier, wait for all threads to come
  size_t b, actions;
  // First round, alloc half of all the bins
  for (b = 0; b < num_bins_; b++) {
    if (!RANDOM(2))
      BinAlloc(binsArr_ + b, RANDOM(maxperbin_size_) + 1, rng());
  }
  actions = RANDOM(ACTIONS_MAX);
  // Second round, alloc and free
  for (size_t i = 0; i <= imax_;) {
    for (size_t j = 0; j < actions; j++) {
      b = RANDOM(num_bins_);
      BinFree(binsArr_ + b);
    }
    i += actions;
    actions = RANDOM(ACTIONS_MAX);
    for (size_t j = 0; j < actions; j++) {
      b = RANDOM(num_bins_);
      BinAlloc(binsArr_ + b, RANDOM(maxperbin_size_) + 1, rng());
    }
    i += actions;
  }

  // Free all bins
  for (b = 0; b < num_bins_; b++)
    BinFree(binsArr_ + b);
  // Increase the rnd_seed_
  rnd_seed_++;
}


// Other memory-check functions
static void mem_init(unsigned char* ptr, size_t size) {
	size_t i, j;

	if (!size) return;
	for (i = 0; i < size; i += 2047)
	{
		j = (size_t)ptr ^ i;
		ptr[i] = j ^ (j>>8);
	}
	j = (size_t)ptr ^ (size - 1);
	ptr[size-1] = j ^ (j>>8);
}

static int mem_check(unsigned char* ptr, size_t size) {
	size_t i, j;

	if (!size) return 0;
	for (i = 0; i < size; i += 2047)
	{
		j = (size_t)ptr ^ i;
		if (ptr[i] != ((j ^ (j>>8)) & 0xFF)) return 1;
	}
	j = (size_t)ptr ^ (size - 1);
	if (ptr[size-1] != ((j ^ (j>>8)) & 0xFF)) return 2;
	return 0;
}

static int zero_check(void* p, size_t size) {
	unsigned* ptr = (unsigned*)p;
	unsigned char* ptr2;

	while (size >= sizeof(*ptr))
	{
		if (*ptr++) return -1;
		size -= sizeof(*ptr);
	}
	ptr2 = (unsigned char*)ptr;
	
	while (size > 0)
	{
		if (*ptr2++) return -1;
		--size;
	}
	return 0;
}

}  // namespace test

  #endif  // MCP_TEST_MEMTESTBINSMGR
