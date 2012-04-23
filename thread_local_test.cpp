#include "callback.hpp"
#include "lock.hpp"
#include "test_unit.hpp"
#include "thread.hpp"
#include "thread_local.hpp"

namespace {

using base::Callback;
using base::ConditionVar;
using base::makeCallableOnce;
using base::makeThread;
using base::Mutex;
using base::ScopedLock;
using base::ThreadLocal;


// A helper class that has a thread-local integer value. We coordinate
// accesses to the locals so that we can use several threads in this
// test.
class Tester {
public:
  Tester() : counter_(0), stopped_(false) {}

  // Fills in 'p' with the address of the integer for this thread and
  // writes 'val' to it. Blocks until stop() is called.
  void init(int** p, int val) {
    *p = local_.getAddr();
    local_.setVal(val);

    {
      ScopedLock l(&m_);
      counter_++;
      cv_.signal();
    }

    {
      ScopedLock l(&m_);
      while (!stopped_) {
        stop_cv_.wait(&m_);
      }
    }
  }

  // Blocks at least until the i-th init()'s have completed.
  void waitOnCounter(int i) {
    ScopedLock l(&m_);
    while (counter_ != i) {
      cv_.wait(&m_);
    }
  }

  // Unblocks all threads waiting inside init().
  void stop() {
    ScopedLock l(&m_);
    stopped_ = true;
    stop_cv_.signalAll();
  }

private:
  ThreadLocal<int> local_;

  Mutex            m_;            // protects state below
  ConditionVar     cv_;           // to signal a counter_ increment
  ConditionVar     stop_cv_;      // to signal stop() was called
  int              counter_;      // # of calls to init(), ie # of threads
  bool             stopped_;      // if stop() was called
};

//
// Test Cases
//

TEST(Basics, OneIntPerThread) {
  Tester tester;
  const int NUM_THREADS = 2;
  int* ints[NUM_THREADS] = {NULL, NULL};
  pthread_t tids[NUM_THREADS];

  // Spin 'i' threads, asking each to fill the 'ints[i]' position with its
  // local integer address and to write 'i' to that address.
  for (int i=0; i<NUM_THREADS; i++) {
    Callback<void>* c = makeCallableOnce(&Tester::init, &tester, &ints[i], i);
    tids[i] = makeThread(c);
  }

  // Block until all threads filled their local integers. Check if the
  // correct values were written after that.
  tester.waitOnCounter(NUM_THREADS);
  for (int i=0; i<NUM_THREADS; i++) {
    EXPECT_EQ(*ints[i], i);
  }

  // Stop all threads.
  tester.stop();
  for (int i=0; i<NUM_THREADS; i++) {
    pthread_join(tids[i], NULL);
  }
}

TEST(Basics, MultipleInstancesInOneThread) {
  ThreadLocal<int> a_int;
  ThreadLocal<int> b_int;
  a_int.setVal(10);
  b_int.setVal(20);
  EXPECT_EQ(a_int.getVal(), 10);
  EXPECT_EQ(b_int.getVal(), 20);
}

TEST(Basics, PointerType) {
  ThreadLocal<int*> p_int;
  p_int.setVal(NULL);
  EXPECT_EQ(p_int.getVal(), reinterpret_cast<int*>(NULL));

  int* new_int = new int(10);
  p_int.setVal(new_int);
  EXPECT_EQ(p_int.getVal(), new_int);
  EXPECT_EQ(*p_int.getVal(), *new_int);
  delete new_int;
}

}  // unnamed namespace

int main(int argc, char *argv[]) {
  return RUN_TESTS(argc, argv);
}
