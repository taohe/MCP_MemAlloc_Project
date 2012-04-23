#include "acceptor.hpp"
#include "io_manager.hpp"
#include "io_service.hpp"

namespace base {

IOService::IOService(int num_workers)
  : io_manager_(new IOManager(num_workers)),
    stats_(num_workers),
    stop_requested_(false),
    stopped_(false) {
 }

IOService::~IOService() {
  // It would be problematic if start() was still running after the
  // call to stop().  But that can't happen, for the following.  If
  // start() was called, we assume that the server will only go out of
  // scope -- what would get us here -- after start() returned, since
  // the latter blocks until a stop() is issued.  If start() wasn't
  // even called, then there's no worries overlapping that and the
  // destructor.
  //
  // After stop() completed, we can assert that
  //   + the io_manager's worker pool threads have all joined
  //   + the acceptor is stopped
  // So the destructor can safely tear down the request serving
  // machinery.
  stop();

  // Only now that there's no more callback activities and the worker
  // threads were joined, we can reckaun the acceptors. Recall, that is
  // not thread safe.
  //
  for (Acceptors::iterator it = acceptors_.begin();
       it != acceptors_.end();
       ++it) {
    delete *it;
  }
  delete io_manager_;
}

void IOService::registerAcceptor(int port, AcceptCallback* cb) {
  acceptors_.push_back(new Acceptor(io_manager_, port, cb));
}

void IOService::start() {
  for (Acceptors::iterator it = acceptors_.begin();
       it != acceptors_.end();
       ++it) {
    (*it)->startAccept();
  }
  io_manager_->poll();  // will block here until stop() is called

  // We must hold here until stop() completed running, which could be
  // *longer* than io_manager.poll() returning. If the destructor,
  // which could be issued after start(), kicks in, it could interfere
  // with stop().

  m_stop_.lock();
  while (! stopped_) {
    cv_stopped_.wait(&m_stop_);
  }
  m_stop_.unlock();
}

void IOService::stop() {
  // Allow only one thread to actually execute the stop(). All others
  // would wait for the latter to finish, if stop() was called
  // concurrently.
  {
    ScopedLock l(&m_stop_);
    if (stop_requested_) {
      while (! stopped_) {
        cv_stopped_.wait(&m_stop_);
      }
      return;
    }
    stop_requested_ = true;
  }

  // Stop accepting new requests.
  for (Acceptors::iterator it = acceptors_.begin();
       it != acceptors_.end();
       ++it) {
    (*it)->close();
  }

  // The stopping sequence waits until all previously enqueued
  // callbacks were served. After which, the worker threads are
  // joined. The poll() call in IOService::start() will be broken
  // somewhere during the stop() call.
  io_manager_->stop();

  // At this stage the internal machinery of the IOService can be
  // destroyed, since all activity was guaranteed to have stopped.
  m_stop_.lock();
  stopped_ = true;
  cv_stopped_.signalAll();
  m_stop_.unlock();
}

bool IOService::stopped() const {
  ScopedLock l(&m_stop_);
  return stop_requested_;
}

}  // namespace base
