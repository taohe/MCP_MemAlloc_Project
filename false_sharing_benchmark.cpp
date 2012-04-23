#include <iostream>
#include <iomanip>

#include "callback.hpp"
#include "thread.hpp"
#include "thread_barrier.hpp"
#include "timer.hpp"

using base::Barrier;
using base::Callback;
using base::makeCallableOnce;
using base::makeThread;
using base::Timer;

// Comment out __attribute__ for a contigous allocation.
struct Val {
  int val __attribute__ ((aligned (64)));
};

struct Vec{
  static const int LIMIT = 24;
  Val vals[LIMIT];
};

struct Adder {
  void add(Vec* vec, Barrier* barrier, int worker_num) {
    barrier->wait();

    int& val = vec->vals[worker_num].val;
    for (int i=0; i<10000000; i++) {
      val *= 3;
    }
  }
};

int main() {
  Adder adder;
  Vec vec;

  std::cout << sizeof(Vec) << std::endl;

  for (int i =  Vec::LIMIT; i <= Vec::LIMIT; i++) {

    Callback<void>** cbs = new Callback<void>*[i];
    Barrier* barrier = new Barrier(i+1);
    for (int j=0; j<i; j++) {
      cbs[j] = makeCallableOnce(&Adder::add, &adder, &vec, barrier, j);
    }

    Timer t;
    pthread_t* threads = new pthread_t[i];
    for (int j=0; j<i; j++) {
      threads[j] = makeThread(cbs[j]);
    }

    // All threads are holding on for this one to open the barrier. We
    // want to measure the time without the overhead to get to this
    // point.
    t.start();
    barrier->wait();
    for (int j=0; j<i; j++) {
      pthread_join(threads[j], NULL);
    }
    t.end();

    delete barrier;
    delete [] cbs;
    delete [] threads;

    std::cout << std::setw(2) << i << " " << t.elapsed() << std::endl;
  }
}
