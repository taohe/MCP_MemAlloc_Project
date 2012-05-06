#ifndef MCP_BASE_LOCK_HEADER
#define MCP_BASE_LOCK_HEADER

#include <stdio.h>    // perror
#include <pthread.h>

// Convenient wrappers around
// + pthread_mutex
// + pthread_cond
// + pthread_rwlock
//
// And a mutex wrapper that locks at construction and unlocks at
// destruction. It can be used for "scope locking."

namespace base {

// Generalized interface for locks in order to
// polymorphically handle spinlocks and mutexes
class Lock {
  public:
    virtual ~Lock() { }
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

class Mutex : public Lock {
public:
  Mutex()         { pthread_mutex_init(&m_, NULL); }
  ~Mutex()        { pthread_mutex_destroy(&m_);}

  void lock()     { pthread_mutex_lock(&m_); }
  void unlock()   { pthread_mutex_unlock(&m_); }

private:
  friend class ConditionVar;

  pthread_mutex_t m_;

  // Non-copyable, non-assignable
  Mutex(Mutex &);
  Mutex& operator=(Mutex&);
};

class ScopedLock {
public:
  explicit ScopedLock(Mutex* lock) : m_(lock) { m_->lock(); }
  ~ScopedLock()   { m_->unlock(); }

private:
  Mutex* m_;

  // Non-copyable, non-assignable
  ScopedLock(ScopedLock&);
  ScopedLock& operator=(ScopedLock&);
};

class ConditionVar {
public:
  ConditionVar()          { pthread_cond_init(&cv_, NULL); }
  ~ConditionVar()         { pthread_cond_destroy(&cv_); }

  void wait(Mutex* mutex) { pthread_cond_wait(&cv_, &(mutex->m_)); }
  void signal()           { pthread_cond_signal(&cv_); }
  void signalAll()        { pthread_cond_broadcast(&cv_); }

  void timedWait(Mutex* mutex, const struct timespec* timeout) {
    pthread_cond_timedwait(&cv_, &(mutex->m_), timeout);
  }


private:
  pthread_cond_t cv_;

  // Non-copyable, non-assignable
  ConditionVar(ConditionVar&);
  ConditionVar& operator=(ConditionVar&);
};

class RWMutex {
public:
  RWMutex()      { pthread_rwlock_init(&rw_m_, NULL); }
  ~RWMutex()     { pthread_rwlock_destroy(&rw_m_); }

  void rLock()   { pthread_rwlock_rdlock(&rw_m_); }
  void wLock()   { pthread_rwlock_wrlock(&rw_m_); }
  void unlock()  { pthread_rwlock_unlock(&rw_m_); }

private:
  pthread_rwlock_t rw_m_;

  // Non-copyable, non-assignable
  RWMutex(RWMutex&);
  RWMutex& operator=(RWMutex&);

};

class ScopedRLock {
public:
  explicit ScopedRLock(RWMutex* lock) : m_(lock) { m_->rLock(); }
  ~ScopedRLock()   { m_->unlock(); }

private:
  RWMutex* m_;

  // Non-copyable, non-assignable
  ScopedRLock(ScopedRLock&);
  ScopedRLock& operator=(ScopedRLock&);
};

class ScopedWLock {
public:
  explicit ScopedWLock(RWMutex* lock) : m_(lock) { m_->wLock(); }
  ~ScopedWLock()   { m_->unlock(); }

private:
  RWMutex* m_;

  // Non-copyable, non-assignable
  ScopedWLock(ScopedWLock&);
  ScopedWLock& operator=(ScopedWLock&);
};

class Notification {
public:
  Notification() : notified_(false) {}
  ~Notification() {} 

  void wait()   { ScopedLock l(&m_); while (! notified_) cv_.wait(&m_); }
  void notify() { ScopedLock l(&m_); notified_ = true; cv_.signal(); }
  void reset()  { ScopedLock l(&m_); notified_ = false; }

private:
  Mutex        m_;
  ConditionVar cv_;
  bool         notified_;
};

} // namespace base

#endif  // MCP_BASE_LOCK_HEADER
