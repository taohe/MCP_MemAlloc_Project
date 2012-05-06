#ifndef MCP_BASE_THREAD_POOL_FAST_HEADER
#define MCP_BASE_THREAD_POOL_FAST_HEADER

#include <list>
#include <queue>
#include <vector>

#include "callback.hpp"
#include "lock.hpp"
#include "thread_pool.hpp"
#include "thread_local.hpp"

namespace base {

using std::list;
using std::queue;
using std::vector;

class ThreadPoolFast : public ThreadPool {
public:

  // ThreadPool interface
  explicit ThreadPoolFast(int num_workers);
  virtual ~ThreadPoolFast();

  virtual void addTask(Callback<void>* task);
  virtual void stop();
  virtual int count() const;

  // Returns the worker ID the call is being issued from. The call
  // must be issued from a worker thread.
  static int ME();
  static void setMEForTest(int i);

private:
  class Worker;

  typedef queue<Callback<void>*> DispatchQueue;
  typedef list<Worker*>          WorkerList;
  typedef vector<pthread_t>      TIDs;

  mutable Mutex                  m_dispatch_;
  ConditionVar                   cv_not_empty_;
  DispatchQueue                  dispatch_queue_;
  WorkerList                     workers_;
  TIDs                           workers_tids_;

  static ThreadLocal<int>        worker_num_;

  void queueWorker(Worker* worker);

  // Non-copyable, non-assignable.
  ThreadPoolFast(const ThreadPoolFast&);
  ThreadPoolFast& operator=(const ThreadPoolFast&);
};

} // namespace base

#endif // MCP_BASE_THREAD_POOL_FAST_HEADER
