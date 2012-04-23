#include <errno.h>     // errno
#include <fcntl.h>
#include <iostream>
#include <stdio.h>     // perror
#include <stdlib.h>    // exit

#include "descriptor_poller.hpp"
#include "io_manager.hpp"
#include "logging.hpp"
#include "thread.hpp"
#include "thread_pool_fast.hpp"

namespace base {

using std::make_pair;
using base::makeCallableMany;

IOManager::IOManager(int num_workers)
  : poller_(new DescriptorPoller),
    worker_pool_(new ThreadPoolFast(num_workers)),
    deleted_desc_(NULL),
    stopped_(false),
    polling_(false) {
  poller_->create();
}

IOManager::~IOManager() {
  stop();
  delete worker_pool_;
  delete poller_;
}

void IOManager::stop() {
  {
    ScopedLock l(&m_stop_);
    if (stopped_) {
      return;
    }

    // Signal the intention to stop and wait for the polling loop to
    // pick it up and break. If we didn't do that, the polling loop
    // would keep adding callbacks to the workers while we were trying
    // to stop the manager.
    stopped_ = true;
    while (polling_) {
      cv_polling_.wait(&m_stop_);
    }
  }

  // Wait for all the workers to finish.
  worker_pool_->stop();

  // Assuming no worker is running, unprotected access to
  // deleted_dest_ is fine here.
  while (deleted_desc_) {
    Descriptor* hold = deleted_desc_;
     deleted_desc_ = deleted_desc_->next_;
    delete hold;
  }
}

bool IOManager::stopped() const {
  ScopedLock l(&m_stop_);
  return stopped_;
}

void IOManager::poll() {
  m_stop_.lock();
  polling_ = true;
  m_stop_.unlock();

  pollBody();
}

Descriptor* IOManager::newDescriptor(int fd,
                                     Callback<void>* read_cb,
                                     Callback<void>* write_cb) {
  Descriptor* descr = new Descriptor(this, fd, read_cb, write_cb);
  poller_->setEvent(fd, descr);
  return descr;
}

void IOManager::delDescriptor(Descriptor* desc) {
  if (desc == NULL) {
    return;
  }

  ScopedLock l(&m_deleted_desc_);
  desc->next_ = deleted_desc_;
  deleted_desc_ = desc;
}

void IOManager::addTimer(double delay, Callback<void>* task) {
  TicksClock::Ticks ts =
    TicksClock::getTicks() + delay * TicksClock::ticksPerSecond();

  m_timer_queue_.lock();
  timer_queue_.insert(make_pair(ts, task));
  m_timer_queue_.unlock();
}

void IOManager::addTask(Callback<void>* task) {
  worker_pool_->addTask(task);
}

void IOManager::pollBody() {
  while (!stopped()) {
    int res = poller_->poll();
    if (res == -1) {
      if (errno != EINTR) {
        perror("Error in epoll_wait ");
        exit(1);
      }
    }

    // Issue the alarm callbacks that are due. We'll clean up the
    // queue shortly.
    m_timer_queue_.lock();
    TicksClock::Ticks now = TicksClock::getTicks();
    TimerQueue::iterator to_execute = timer_queue_.begin();
    while (to_execute != timer_queue_.end()) {
      if (to_execute->first > now) {
        break;
      }
      worker_pool_->addTask(to_execute->second);
      timer_queue_.erase(to_execute++);
    }
    m_timer_queue_.unlock();

    int e;
    Descriptor* desc;
    for (int i = 0; i < res; i++) {
      poller_->getEvents(i, &e, &desc);
      if (e & (DescriptorPoller::DP_ERROR | DescriptorPoller::DP_READ_READY)) {
        desc->readIfWaiting();
      }
      if (e & (DescriptorPoller::DP_ERROR | DescriptorPoller::DP_WRITE_READY)) {
        desc->writeIfWaiting();
      }
    }

    Descriptor* to_delete = NULL;
    m_deleted_desc_.lock();
    to_delete = deleted_desc_;
    deleted_desc_ = NULL;
    m_deleted_desc_.unlock();
    while (to_delete) {
      Descriptor* hold = to_delete;
      to_delete = to_delete->next_;
      delete hold;
    }
  }

  m_stop_.lock();
  polling_ = false;
  cv_polling_.signal();
  m_stop_.unlock();
}

// ************************************************************
// Descriptor Implementetion
//

Descriptor::Descriptor(IOManager* io_manager,
                       int fd,
                       Callback<void>* read_cb,
                       Callback<void>* write_cb)
  : io_manager_(io_manager),
    fd_(fd),
    closed_(false),
    read_cb_(read_cb),   // takes ownership
    write_cb_(write_cb), // takes ownership
    can_read_(false),
    can_write_(false),
    waiting_read_(false),
    waiting_write_(false) {
  int flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

Descriptor::~Descriptor() {
  ScopedLock l(&m_);
  delete read_cb_;
  delete write_cb_;
  read_cb_ = NULL;
  write_cb_ = NULL;
}

void Descriptor::setUpCalls(Callback<void>* read_cb, Callback<void>* write_cb) {
  m_.lock();
  Callback<void>* read_cb_hold = read_cb_;
  Callback<void>* write_cb_hold = write_cb_;
  read_cb_ = read_cb;
  write_cb_ = write_cb;
  m_.unlock();

  delete read_cb_hold;
  delete write_cb_hold;
}

void Descriptor::readWhenReady() {
  bool ready_now = false;

  m_.lock();
  if (can_read_) {
    ready_now = true;
    can_read_ = false;
  } else {
    waiting_read_ = true;
  }
  m_.unlock();

  if (read_cb_ && ready_now) {
    workerPool()->addTask(read_cb_);
  }
}

void Descriptor::writeWhenReady() {
  bool ready_now = false;

  m_.lock();
  if (can_write_) {
    ready_now = true;
    can_write_ = false;
  } else {
    waiting_write_ = true;
  }
  m_.unlock();

  if (write_cb_ && ready_now) {
    workerPool()->addTask(write_cb_);
  }
}

void Descriptor::readIfWaiting() {
  bool schedule_now = false;

  m_.lock();
  if (waiting_read_) {
    schedule_now = true;
    waiting_read_ = false;
  } else {
    can_read_ = true;
  }
  m_.unlock();

  if (read_cb_ && schedule_now) {
    workerPool()->addTask(read_cb_);
  }
}

void Descriptor::writeIfWaiting() {
  bool schedule_now = false;

  m_.lock();
  if (waiting_write_) {
    schedule_now = true;
    waiting_write_ = false;
  } else {
    can_write_ = true;
  }
  m_.unlock();

  if (write_cb_ && schedule_now) {
    workerPool()->addTask(write_cb_);
  }
}

} // namespace base
