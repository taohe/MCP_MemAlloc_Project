#include <cstdlib>
#include <sys/time.h>  // gettimeofday

#include "callback.hpp"
#include "logging.hpp"
#include "thread.hpp"

#include "thread_pool_fast.hpp"

namespace base {

using base::Callback;
using base::makeCallableOnce;

static __thread bool last_worker_ = false;

ThreadLocal<int> ThreadPoolFast::worker_num_;

//
// Internal Worker Class
//

class ThreadPoolFast::Worker {
public:
  Worker(ThreadPoolFast*);
  ~Worker();

  void workerLoop(int instance);
  void assignTask(Callback<void>* task);

private:
  ThreadPoolFast* my_pool_;        // not owned here

  Mutex           m_;
  ConditionVar    cv_has_task_;
  bool            has_task_;
  Callback<void>* task_;

};

ThreadPoolFast::Worker::Worker(ThreadPoolFast* pool)
  : my_pool_(pool),
    has_task_(false),
    task_(NULL) {
}

ThreadPoolFast::Worker::~Worker() {
}

void ThreadPoolFast::Worker::workerLoop(int instance) {
  worker_num_.setVal(instance);

  while (true) {

    // Wait until I know my task. Because a task is assigned to this
    // worker, we assume it left the free worker's pool.
    {
      ScopedLock l(&m_);

      while (!has_task_) {
        cv_has_task_.wait(&m_);
      }
      has_task_ = false;
    }

    // A NULL task is considered a request to stop this worker.
    if (task_ == NULL) {
      delete this;
      break;
    }

    // If this worker is executing the ThreadPool tear down,
    // i.e. stop(), the latter will notify this thread is the last
    // worker, after waiting for all other worker threads to join.

    (*task_)();  // would self-delete if once-run task

    if (last_worker_) {
      delete this;
      break;
    }

    // Return the worker to the free worker's pool
    my_pool_->queueWorker(this);
  }
}

void ThreadPoolFast::Worker::assignTask(Callback<void>* task) {
  ScopedLock l(&m_);

  task_ = task;
  has_task_ = true;
  cv_has_task_.signal();
}

//
//  ThreadPoolFast Definitions
//

ThreadPoolFast::ThreadPoolFast(int num_workers) {
  for (int i = 0; i < num_workers; i++) {
    Worker* worker = new Worker(this);
    Callback<void>* body = makeCallableOnce(&Worker::workerLoop, worker, i);
    workers_tids_.push_back(makeThread(body));
    queueWorker(worker);
  }
}

ThreadPoolFast::~ThreadPoolFast() {
  m_dispatch_.lock();
  while (! dispatch_queue_.empty()) {
    Callback<void>* task = dispatch_queue_.front();
    dispatch_queue_.pop();
    if (task && task->once()) {
      delete task;
    }
  }
  m_dispatch_.unlock();
}

void ThreadPoolFast::stop() {
  // Issue a stop request callback for each worker thread. If the
  // stop() is being issued from one of the workers itself, one of the
  // stop callbacks won't be consummed. The destructor would dispose
  // of it.
  for (size_t i = 0; i < workers_tids_.size(); i++) {
    addTask(NULL);
  }

  bool exit_last_worker = false;
  const size_t num_workers = workers_tids_.size();
  for (size_t i = 0; i < num_workers; ++i) {
    if (pthread_self() == workers_tids_[i]) {
      exit_last_worker = true;
    } else {
      pthread_join(workers_tids_[i], NULL);
    }
  }

  if (exit_last_worker) {
    last_worker_ = true;
  }
}

void ThreadPoolFast::queueWorker(Worker* worker) {
  ScopedLock l(&m_dispatch_);

  // If there are tasks waiting, pick the worker right away; don't
  // bother putting it back in the pool.
  if (! dispatch_queue_.empty()) {
    worker->assignTask(dispatch_queue_.front());
    dispatch_queue_.pop();
  } else {
    workers_.push_front(worker);
  }
}

void ThreadPoolFast::addTask(Callback<void>* task) {
  ScopedLock l(&m_dispatch_);

  if (! workers_.empty()) {
    Worker* worker = workers_.front();
    workers_.pop_front();
    worker->assignTask(task);
    return;
  }

  dispatch_queue_.push(task);
}

int ThreadPoolFast::count() const {
  ScopedLock l(&m_dispatch_);
  return dispatch_queue_.size();
}

/*static*/
int ThreadPoolFast::ME() {
  return worker_num_.getVal();
}

void ThreadPoolFast::setMEForTest(int i) {
  worker_num_.setVal(i);
}

} // namespace base
