#include "callback.hpp"
#include "lock.hpp"
#include "thread_pool_fast.hpp"
#include "thread_pool_normal.hpp"
#include "test_unit.hpp"

namespace {

using base::Callback;
using base::makeCallableMany;
using base::makeCallableOnce;
using base::Mutex;
using base::Notification;
using base::ScopedLock;
using base::ThreadPool;
using base::ThreadPoolNormal;

struct Counter {
  Counter() : i(0) { }
  void Incr() { ScopedLock l(&m); ++i; }
  void SlowIncr() { usleep(10000 /*10ms*/); Incr(); }
  int Get() const { ScopedLock l(&m); return i; }

  mutable Mutex m;
  int i;
};

struct Stopper {
  Stopper(ThreadPool* p) : p_(p) {}
  ~Stopper() {}
  void stop() { p_->stop(); n_.notify(); }
  void wait() { n_.wait(); }

  ThreadPool* p_;
  Notification n_;
};

//
// Test Cases
//

TEST(Basics, Sequential) {
  Counter counter;
  ThreadPool* pool = new ThreadPoolNormal(1);

  const int num_repeats = 10;
  Callback<void>* task = makeCallableMany(&Counter::Incr, &counter);
  for (int i = 0; i < num_repeats ; i++) {
    pool->addTask(task);
  }

  // Wait for all the tasks to finish and check counter.
  pool->stop();
  EXPECT_EQ(counter.Get(), num_repeats);
  delete pool;
  delete task;
}

TEST(Basics, StopIssuedInsideThePool) {
  Counter counter;
  ThreadPool* pool = new ThreadPoolNormal(1);
  Stopper* stopper = new Stopper(pool);

  Callback<void>* task = makeCallableMany(&Counter::Incr, &counter);
  const int num_repeats = 10;
  for (int i = 0; i < num_repeats ; i++) {
    pool->addTask(task);
  }

  // Ask one of the pool's worker thread to issue the stop request and
  // wait for the pool to process it.
  Callback<void>* stop = makeCallableOnce(&Stopper::stop, stopper);
  pool->addTask(stop);
  stopper->wait();

  EXPECT_EQ(counter.Get(), num_repeats);
  delete stopper;
  delete pool;
  delete task;
}

TEST(Basics, Concurrency) {
  Counter counter;
  ThreadPool* pool = new ThreadPoolNormal(2);

  const int num_repeats = 10;
  Callback<void>* task = makeCallableMany(&Counter::Incr, &counter);
  for (int i = 0; i < num_repeats ; i++) {
    pool->addTask(task);
  }

  // There should be pending tasks because SlowIncre takes its time.
  EXPECT_GT(num_repeats, counter.Get());

  // Wait for all the tasks to finish and check counter.
  pool->stop();
  EXPECT_EQ(counter.Get(), num_repeats);
  delete pool;
  delete task;
}

} // unnammed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
