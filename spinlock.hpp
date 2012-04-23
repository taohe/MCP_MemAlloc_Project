#ifndef MCP_BASE_SPINLOCK_HEADER
#define MCP_BASE_SPINLOCK_HEADER

#include <ctime>

namespace base {

// A spinning lock with backoff.
//
// This class predates C++11 atomics (and can be compiled in early
// versions). We use GCC built-in atomics instead. The lock state is
// represented by a boolean and all accesses to that state are
// explicit in terms of atomicity and memory ordering expected.
//
class Spinlock {
public:
  Spinlock() : locked_(false) {}

  ~Spinlock() { }

  void lock() {
    // Fast path for when lock is available.
    if (! loadLockState() && !__sync_lock_test_and_set(&locked_, true)) {
      return;
    }

    // Spin for a bitm waiting for the lock.
    int wait = 1000;
    while ((wait-- > 0) && loadLockState()) {}

    // If failed to grab lock, sleep.
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = 5000000;
    while (__sync_lock_test_and_set(&locked_, true)) {
      nanosleep(&t, NULL);
    }
  }

  void unlock() {
    __sync_lock_release(&locked_);
  }

private:
  int locked_;

  // We force a memory access to 'locked_' without any barrier
  // guarantees. (This would equate to a load with
  // 'memory_order_relaxed'.)
  int loadLockState() const volatile {
    return locked_;
  }

  // Non-copyable, non-assignable
  Spinlock(Spinlock&);
  Spinlock& operator=(Spinlock&);
};

}  // namespace base

#endif  //  MCP_BASE_SPINLOCK_HEADER
