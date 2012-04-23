#ifndef MCP_BASE_THREAD_POOL_HEADER
#define MCP_BASE_THREAD_POOL_HEADER

#include "callback.hpp"

namespace base {

// Abstract base class for experimenting with thread-pool strategies.
class ThreadPool {
public:
  // Cleans up any pending callbacks that weren't executed. Pending
  // callbacks may have been added after stop() was called if a running
  // worker issued new addTask()s.
  //
  // REQUIRES: stop() have completed executing.
  virtual ~ThreadPool() {}

  // Requests the execution of 'task' on an undetermined worker thread.
  virtual void addTask(Callback<void>* task) = 0;

  // Waits for all the workers to finish processing the ongoing tasks
  // and stop then stop the pool. This call may be issued from within
  // a worker thread itself.
  virtual void stop() = 0;

  // accessors

  // Returns the current size of the dispatch queue (pending tasks).
  virtual int count() const = 0;
};

} // namespace base

#endif // MCP_BASE_THREAD_POOL_HEADER
