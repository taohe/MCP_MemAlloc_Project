#ifndef MCP_BASE_THREAD_POOL_NORMAL_HEADER
#define MCP_BASE_THREAD_POOL_NORMAL_HEADER

#include <queue>
#include <vector>

#include "callback.hpp"
#include "lock.hpp"
#include "thread_pool.hpp"

namespace base {

using std::queue;
using std::vector;

class ThreadPoolNormal : public ThreadPool {
public:
  // ThreadPoolNormal interface
  explicit ThreadPoolNormal(int num_workers);
  virtual ~ThreadPoolNormal();

  virtual void addTask(Callback<void>* task);
  virtual void stop();
  virtual int count() const;

private:
  typedef queue<Callback<void>*> DispatchQueue;

  vector<pthread_t>    workers_;

  mutable Mutex        m_dispatch_;
  DispatchQueue        dispatch_queue_;
  ConditionVar         cv_not_empty_;

  void workerLoop();

  // Non-copyable, non-assignable.
  ThreadPoolNormal(const ThreadPoolNormal&);
  ThreadPoolNormal& operator=(const ThreadPoolNormal&);
};

} // namespace base

#endif // MCP_BASE_THREAD_POOL_NORMAL_HEADER
