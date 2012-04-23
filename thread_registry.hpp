#ifndef MCP_BASE_THREAD_REGISTRY_BASE
#define MCP_BASE_THREAD_REGISTRY_BASE

#include <pthread.h>
#include <tr1/unordered_set>

#include "lock.hpp"

namespace base {

using std::tr1::unordered_set;

// A singleton list of all threads IDS that where created using
// 'makeThread'. The list is used by fatal signal handler, which will
// try to obtain the stack trace of each of the threads listed here.
//
// The class is thread-safe.
//
class ThreadRegistry {
public:
  ~ThreadRegistry();

  // Return ThreadRegistry's singleton instnce.
  static ThreadRegistry* instance();

  // Adds 'tid' to the registry and returns true, if tid was not
  // registered before. Return false otherwise.
  bool registerThread(pthread_t tid);

  // Removes 'tid' from the registry and returns true, if tid was
  // registered before. Returns false otherwise.
  bool unregisterThread(pthread_t tid);

  // Fill 'tids' with all the registered threads.
  void getAllThreadIDs(unordered_set<pthread_t>* tids) const;

private:
  typedef unordered_set<pthread_t> ThreadIDs;

  // Singleton initialization state.
  static pthread_once_t init_control_;
  static ThreadRegistry* instance_;

  // A list of all known thread IDs protected by m_.
  mutable Mutex m_;
  ThreadIDs thread_ids_;

  // This is a singleton class. To access it, call 'instance()'.
  ThreadRegistry();
  static void init();
};

} // namespace base

#endif // MCP_BASE_THREAD_REGISTRY_BASE
