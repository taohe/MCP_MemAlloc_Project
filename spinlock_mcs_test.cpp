#include <pthread.h>

#include <algorithm>
#include <iostream>

#include "callback.hpp"
#include "spinlock.hpp"
#include "spinlock_mcs.hpp"
#include "test_unit.hpp"
#include "thread.hpp"
#include "thread_barrier.hpp"

namespace {

using base::Barrier;
using base::Callback;
using base::makeCallableOnce;
using base::makeThread;
using base::Spinlock;
using base::SpinlockMCS;

template <typename LockType>
class Tester {
public:
  Tester(int* counters, int len, Barrier* barrier)
    : counters_(counters),
      len_(len),
      b_(barrier),
      stopped_(false) { }

  ~Tester() {}

  void start(int pos, LockType* l) {
    b_->wait();
    while (! isStopped()) {
      l->lock();
      counters_[pos] += 1;
      l->unlock();
    }
  }

  void stop() {
    stopped_ = true;
  }

private:
  int* counters_;  // not owned here
  int len_;
  Barrier* b_;     // not owned here
  bool stopped_;

  bool isStopped() const volatile {
    return stopped_;
  }
};


TEST(Concurrency, Fairness) {
  const int NUM_THREADS = 4;

  Spinlock s;
  SpinlockMCS q;
  Barrier b(NUM_THREADS+1);
  pthread_t tids[NUM_THREADS];
  int counter[NUM_THREADS];

  // Setup NUM_THREADS threads that will each increment a position of
  // 'counter'. If the lock used is fair, each position should have an
  // similar count at the end of the test.
  Tester<SpinlockMCS> tester(&counter[0], NUM_THREADS, &b);
  for (int i = 0; i < NUM_THREADS; ++i) {
    counter[i] = 0;
    Callback<void>* cb = makeCallableOnce(&Tester<SpinlockMCS>::start,
                                          &tester,
                                          i,
                                          &q);
    tids[i] = makeThread(cb);
  }

  // Release all threads from the barrier and give them some time to
  // compete for the counters.
  b.wait();
  sleep(1);
  tester.stop();

  // Wait for the threads to respond and check whether the threads ran
  // approximately the same number of times.
  for (int i = 0; i < NUM_THREADS; ++i) {
    pthread_join(tids[i], NULL);
  }
  std::cout << counter[0] << " ";
  for (int i = 1; i < NUM_THREADS; ++i) {
    std::cout << counter[i] << " ";
    int diff = std::min(counter[i], counter[i-1]);
    int pct = diff * 100 / std::max(counter[i], counter[i-1]);
    EXPECT_GT(pct, 95);
  }
}

}  // unnamed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
