#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "callback.hpp"
#include "thread_pool_normal.hpp"
#include "thread_pool_fast.hpp"
#include "timer.hpp"

namespace {

using std::cout;
using std::endl;
using std::istringstream;
using std::left;
using std::setw;

using base::Callback;
using base::makeCallableMany;
using base::ThreadPoolNormal;
using base::ThreadPoolFast;
using base::Timer;

const int NUM_THREADS = 24;
const int NUM_CALLS = 40000; // 400000;

template<typename PoolType>
struct Counters {

  // XXX
  // There's preparation to avoid false sharing here but the code
  // is still not taking advantage of it.
  struct PaddedCounter {
    int i;
    double d;
    char pad[64 - sizeof(int) - sizeof(double)];
  } padded_counter[NUM_THREADS];

  void inc() {
    PaddedCounter& c = padded_counter[0];  // XXX false sharing
    c.i++;
    c.d = c.i * 2.1;
  }

  void slowInc() {
    PaddedCounter& c = padded_counter[0]; // XXX false sharing
    c.i++;
    for (int i = 0; i < 10000; i++) {
      c.d = c.i * i / 2.1;
    }
  }

  void print() {
    int total = 0;
    for (int j=0; j<NUM_THREADS; j++) {
      const int& count = padded_counter[j].i;
      total += count;
      std::cout << "Worker " << j << " " << count << std::endl;
    }
    std::cout << "Total " << total << " requests" << std::endl;
  }
};

template<typename PoolType>
void FastConsumer() {
  int num_threads = 4;
  std::cout << "Fast Consumer:\t";

  while (num_threads <= NUM_THREADS) {
    PoolType* pool = new PoolType(num_threads);
    Counters<PoolType>* counters = new Counters<PoolType>;
    Callback<void>* task = makeCallableMany(&Counters<PoolType>::inc, counters);

    Timer timer;
    timer.start();

    for (int i=0; i<NUM_CALLS; i++) {
      pool->addTask(task);
    }
    pool->stop(); // wait to finish the tasks

    timer.end();
    std::cout << std::setw(8) << std::left << timer.elapsed() << " ";

    delete task;
    delete counters;
    delete pool;

    num_threads += 4;
  }

  std::cout << std::endl;
}

template<typename PoolType>
void SlowConsumer() {
  int num_threads = 4;
  std::cout << "Slow Consumer:\t";

  while (num_threads <= NUM_THREADS) {
    PoolType* pool = new PoolType(num_threads);
    Counters<PoolType>* counters = new Counters<PoolType>;
    Callback<void>* task =
      makeCallableMany(&Counters<PoolType>::slowInc, counters);

    Timer timer;
    timer.start();

    for (int i=0; i<NUM_CALLS; i++) {
      pool->addTask(task);

      if (i%1000 == 0) {
        while (pool->count() > NUM_THREADS*4) {
          usleep(500);
        }
      }
    }
    pool->stop();  // wait to finish the tasks

    timer.end();
    std::cout << std::setw(8) << std::left << timer.elapsed() << " ";

    delete task;
    delete counters;
    delete pool;

    num_threads += 4;
  }

  std::cout << std::endl;
}

}  // unnamed namespace

void usage(int argc, char* argv[]) {
  std::cout << "Usage: " << argv[0] << " [1 | 2]" << std::endl;
  std::cout << "  1 is normal thread pool" << std::endl;
  std::cout << "  2 is fast thread pool" << std::endl;
  std::cout << "  defaul is to run both " << std::endl;
}

int main(int argc, char* argv[]) {
  bool all = false;
  bool num[4] = {false};
  if (argc == 1) {
    all = true;
  } else if (argc == 2) {
    istringstream is(argv[1]);
    int i = 0;
    is >> i;
    if (i>0 && i<=4) {
      num[i-1] = true;
    } else {
      usage(argc, argv);
      return -1;
    }
  } else {
    usage(argc, argv);
    return -1;
  }

  // no queueing of tasks really
  if (all || num[0]) {
    FastConsumer<ThreadPoolNormal>();
  }
  if (all || num[1]) {
    FastConsumer<ThreadPoolFast>();
  }

  // force queue building
  if (all || num[0]) {
    SlowConsumer<ThreadPoolNormal>();
  }
  if (all || num[1]) {
    SlowConsumer<ThreadPoolFast>();
  }

  return 0;
}
