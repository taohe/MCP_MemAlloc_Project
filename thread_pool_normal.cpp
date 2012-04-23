#include <sys/time.h>  // gettimeofday

#include "logging.hpp"
#include "thread.hpp"

#include "thread_pool_normal.hpp"

namespace base {

static __thread bool last_worker_ = false;

ThreadPoolNormal::ThreadPoolNormal(int num_workers) {
  for (int i = 0; i < num_workers; ++i) {
    Callback<void>* body = makeCallableOnce(&ThreadPoolNormal::workerLoop,
                                            this);
    workers_.push_back(makeThread(body));
  }
}

ThreadPoolNormal::~ThreadPoolNormal() {
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

void ThreadPoolNormal::stop() {
  // We use the queue itself to signal to the workers they are
  // supposed to stop. Upon seeing the NULL task, a worker would
  // return and the join below would pick that.
  //
  // If however we are executing stop from within a worker, we
  // want that worker to return immediately after executing stop().
  {
    ScopedLock l(&m_dispatch_);
    for (size_t i = 0; i < workers_.size(); ++i) {
      dispatch_queue_.push(NULL);
    }
    cv_not_empty_.signalAll();
  }

  bool exit_last_worker = false;
  const int num_workers = workers_.size();
  for (int i = 0; i < num_workers; ++i) {
    if (pthread_self() == workers_[i]) {
      exit_last_worker = true;
    } else {
      pthread_join(workers_[i], NULL);
    }
  }

  if (exit_last_worker) {
    last_worker_ = true;
  }
}

void ThreadPoolNormal::addTask(Callback<void>* task) {
  ScopedLock l(&m_dispatch_);
  dispatch_queue_.push(task);
  cv_not_empty_.signal();
}

void ThreadPoolNormal::workerLoop() {
  Callback<void>* task = NULL;
  while (true) {
    {
      ScopedLock l(&m_dispatch_);

      while (dispatch_queue_.empty()) {
        cv_not_empty_.wait(&m_dispatch_);
      }

      task = dispatch_queue_.front();
      dispatch_queue_.pop();
    }

    if (task == NULL) {
      LOG(LogMessage::NORMAL) << "worker stopped";
      return;
    }

    // If this worker is executing the ThreadPool tear down,
    // i.e. stop(), the latter will notify this thread is the last
    // worker, after waiting for all other worker threads to join.

    (*task)(); // would self-delete if once-run task

    if (last_worker_) {
      return;
    }

  }
}

int ThreadPoolNormal::count() const {
  ScopedLock l(&m_dispatch_);
  return dispatch_queue_.size();
}

} // namespace base
