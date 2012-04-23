#include <iostream>

#include "memtest_binsmgr.hpp"
#include "callback.hpp"
#include "thread.hpp"
#include "thread_barrier.hpp"
#include "ticks_clock.hpp"

#define MEMORYLIMIT (1ULL << 26)
#define NTOTPRINT 50

using test::MemTestBinsMgr;
using base::Callback;
using base::makeCallableOnce;
using base::makeThread;
using base::Barrier;
using base::TicksClock;

uint64_t memAllocBenchmark(const int N_THREADS, int maxsizeperbin) {
  size_t imax = 10000;
  const int rounds = 100;
  size_t numbins = MEMORYLIMIT / (N_THREADS * maxsizeperbin);
  MemTestBinsMgr** testers = (MemTestBinsMgr**) malloc
    (sizeof(MemTestBinsMgr*) * N_THREADS);
  Callback<void>** bodies = (Callback<void>**) malloc
    (sizeof(Callback<void>*) * N_THREADS);
  pthread_t* tids =new pthread_t[N_THREADS];
  TicksClock::Ticks total = 0;


  for (int i = 0; i < rounds; ) {
    Barrier b(N_THREADS + 1);  // Within block, re-fresh every time

    for (int j = 0; j < N_THREADS; j++) {
      testers[j] = new MemTestBinsMgr(maxsizeperbin, numbins, imax,
          j + rounds, &b);
      bodies[j] = makeCallableOnce(&MemTestBinsMgr::MallocTest, testers[j]);
    }
    // Create all child-threads
    for (int j = 0; j < N_THREADS; j++) {
      tids[j] = makeThread(bodies[j]);
    }
    b.wait();  // Release all threads now

    // Start timing
    TicksClock::Ticks start = TicksClock::getTicks();
    // wait for all to complete
    for (int j = 0; j < N_THREADS; j++) {
      pthread_join(tids[j], NULL);
    }
    // End of timing
    TicksClock::Ticks diff = TicksClock::getTicks() - start;
    total += diff;
    // std::cout << "Ticks used= " << diff << std::endl;

    i++;
    // if (!(i % NTOTPRINT))
    //   std::cout << i << "rounds have been finished!\n";
    // if (i == 1) {
    //   std::cout << i << "rounds have been finished!\n";
    //   std::cout << "Ticks used= " << diff << std::endl;
    // }

    // free testers and callbacks
    for (int j = 0; j < N_THREADS; j++) {
      delete testers[j];
    }
  }
  // std::cout << "Average ticks: " << total / static_cast<uint64_t>(rounds)
  //   << std::endl;

  // free memory
  free(testers);
  free(bodies);
  delete [] tids;

  return total / static_cast<uint64_t>(rounds);
}

void varyAllocSize(int N_THREADS) {
  int allocsize = 6;
  const int maxsize = 20, interval = 2;
  uint64_t ticksdiff;

  std::cout << "Allocsize(log 2)   Ticks\n";
  while (allocsize <= maxsize) {
    ticksdiff = memAllocBenchmark(N_THREADS, (1 << allocsize));
    std::cout << allocsize << "   " << ticksdiff << std::endl;
    allocsize += interval;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: ./build/release/memalloc_benchmark_?  #ofthreads\n";
    return 0;
  }

  varyAllocSize(atoi(argv[1]));
  return 0;
}
