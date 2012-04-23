#ifndef MCP_BASE_SPINLOCK_MCS_HEADER
#define MCP_BASE_SPINLOCK_MCS_HEADER

#include <cstddef> // NULL

namespace base {

class SpinlockMCS {
public:
  SpinlockMCS()
    : tail_(NULL) { }

  ~SpinlockMCS() {
    delete qnode_;
    qnode_ = NULL;
  }

  void lock() {
    if (qnode_ == NULL) {
      qnode_ = new Node();
    }
    qnode_->next = NULL;
    Node* previous = __sync_lock_test_and_set(&tail_, qnode_);
    if (previous != NULL) {
      qnode_->locked = true;

      // Stricly speaking, we need a compiler barrier here if we want
      // to make sure the compiler is not going to reorder the writes
      // to 'locked' and 'next'. Moreover, if the hardware reordered
      // writes, then we can use just a release barrier (which is a
      // compiler barrier as well) instead of a full barrier. The
      // release says that we want to send the write to 'locked' out
      // to other processors but do not necessarily want to take
      // anyting from them in at this juncture. In any case, the full
      // barrier below is overkill: Intels do not reorder writes and
      // (checking by hand) gcc 4.4 is not reordering these two writes
      // either. In the context of this exercise, that's good enough
      // for us. (C++11 would make expressing the barriers easier.)

      // __sync_synchronize();

      previous->next = qnode_;
      while (qnode_->loadLockState()) {}
    }

    // We want an acquire barrier at this point so that we are sure to
    // see all values manipulated in the previous critical section
    // that were released by the barrier in the unlock. The full
    // barrier below is more than we wanted. Again, in the context of
    // this exercise, we do not want to produce a special barrier (an
    // acquire one in this case) by hand. (And, again, C++11 would make
    // that easy).
    __sync_synchronize();
  }

  void unlock() {
    if (qnode_->next == NULL) {
      if (__sync_bool_compare_and_swap(&tail_, qnode_, NULL)) {
        return;
      }
      while (qnode_->loadNextState() == NULL) {}
    }

    // We want to set qnode_->next->locked to false. We use gcc's
    // atomic-write-0-with-a-release-barrier here, since that's
    // exactly what we want.
    __sync_lock_release(&qnode_->next->locked);
  }

private:
  struct Node {
    bool locked;
    Node* next;

    Node() : locked(true), next(NULL) {}

    bool loadLockState() const volatile {
      return locked;
    }

    Node* loadNextState() const volatile {
      return next;
    }
  };

  Node* tail_;
  static __thread Node* qnode_;

  // Non-copyable, non-assignable
  SpinlockMCS(SpinlockMCS&);
  SpinlockMCS& operator=(SpinlockMCS&);
};

}  // namespace base

#endif // MCP_BASE_SPINLOCK_MCS_HEADER
