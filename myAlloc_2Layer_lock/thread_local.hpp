#ifndef MCP_BASE_THREAD_LOCAL_HEADER
#define MCP_BASE_THREAD_LOCAL_HEADER

#include "pthread.h"
#include "string.h"

namespace base {

// A ThreadLocal is a wrapper around pthread's Thread Local
// Storage. Each thread accessing a ThreadLocal<T> will have its own
// instance of T.
//
// The class is thread-safe.
//
// usage:
//   class HasALocal {
//   ...
//   private:
//     ThreadLocal<int> my_local_;
//   };
//
//   void HasALocal::aMethod() {
//     my_local_.setVal(<value for this thread>);
//
#define MAXNUMOFTHREADS 16

template<typename T>
class ThreadLocal {
public:
  // Sets up a key by which this local storage is known and sets
  // 'destroyLocalKey' to be run at each thread exit.
  explicit ThreadLocal();
  ~ThreadLocal() {}

  // Releases resources used by a thread that had local storage. Called
  // automatically when that thread exited.
  static void destroyLocalKey(void* thread_state);

  // Returns the value stored for the caller's threads.
  T getVal();

  // Returns the address of the local storage for the caller's thread.
  //
  // NOTE: Use the address with caution. It is only guaranteed to be
  // valid while the "owner" thread is running.
  T* getAddr();

  // Stores 'val' into the local storage for the caller's thread.
  void setVal(const T& val);
  // Get base addr of 'storageArr[]', for testing
  const T* getBaseAddr() const { return storageArr; }

private:
  // Pthread "key" by which this variable is going to be known.
  mutable pthread_key_t local_key_;
  mutable T             storageArr[MAXNUMOFTHREADS];
  int                   curavail_slot_;

  // If the calling thread has already allocated its private instance
  // of T, then return that address. Otherwise, allocate a new
  // instance first.
  T* getLocalState();

  // Non-copyable, non-assignable
  ThreadLocal(const ThreadLocal&);
  ThreadLocal& operator=(const ThreadLocal&);
};

template<typename T>
ThreadLocal<T>::ThreadLocal() : curavail_slot_(0) {
  memset(storageArr, -1, MAXNUMOFTHREADS * sizeof(T));
  pthread_key_create(&local_key_, destroyLocalKey);
}

template<typename T>
void ThreadLocal<T>::destroyLocalKey(void* thread_state) {
  if (thread_state != NULL) {
    T* p = reinterpret_cast<T*>(thread_state);
    *p = 0;   // Delete is reset to zero
  }
}

template<typename T>
T* ThreadLocal<T>::getLocalState() {
  T* local_state = reinterpret_cast<T*>(pthread_getspecific(local_key_));
  if (local_state == NULL) {
    local_state = storageArr + __sync_fetch_and_add(&curavail_slot_, 0x1);
    pthread_setspecific(local_key_, local_state);
  }
  return local_state;
}

template<typename T>
T ThreadLocal<T>::getVal() {
  return *getLocalState();
}

template<typename T>
void ThreadLocal<T>::setVal(const T& val) {
  T* p = getLocalState();
  *p = val;
}

template<typename T>
T* ThreadLocal<T>::getAddr() {
  return getLocalState();
}

}  // namespace base

#endif // MCP_BASE_THREAD_LOCAL_HEADER
