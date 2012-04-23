#if defined(LINUX)

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "descriptor_poller.hpp"

namespace base {

struct DescriptorPoller::InternalPoller {
  static const int MAX_FDS_PER_POLL = 1024;

  int fd_;
  struct epoll_event events_[MAX_FDS_PER_POLL];

  InternalPoller() : fd_(-1) {}
};

DescriptorPoller::DescriptorPoller() {
  poller_ = new InternalPoller;
}

DescriptorPoller::~DescriptorPoller() {
  if (poller_->fd_ == -1) {
    close(poller_->fd_);
  }

  delete poller_;
}

void DescriptorPoller::create() {
  poller_->fd_ = epoll_create(InternalPoller::MAX_FDS_PER_POLL);
  if (poller_->fd_ < 0) {
    perror("Can't create epoll");
    exit(1);
  }
}

void DescriptorPoller::setEvent(int fd, Descriptor* descr) {
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
  ev.data.ptr = reinterpret_cast<void*>(descr);
  int rc = epoll_ctl(poller_->fd_, EPOLL_CTL_ADD, fd, &ev);
  if (rc != 0) {
    perror("Can't add epoll descriptor: ");
    exit(1);
  }
}

int DescriptorPoller::poll() {
  int res;
  for (;;) {

    res = epoll_wait(poller_->fd_,
                     poller_->events_, InternalPoller::MAX_FDS_PER_POLL,
                     100 /* ms */);

    if (res >= 0) {
      break;
    }

    // An accepted error here is epoll breaking out because of an
    // interrupt.
    if (errno == EINTR) {
      continue;
    } else {
      perror("Error in epoll_wait ");
      exit(1);
    }
  }
  return res;
}

void DescriptorPoller::getEvents(int i, int* events, Descriptor** descr) {
  *events &= 0x00000000;
  *descr = reinterpret_cast<Descriptor*>(poller_->events_[i].data.ptr);

  if (poller_->events_[i].events & EPOLLERR) {
    *events |= DP_ERROR;
    return;
  }

  if (poller_->events_[i].events & (EPOLLHUP | EPOLLIN)) {
    *events |= DP_READ_READY;
  }
  if (poller_->events_[i].events & (EPOLLHUP | EPOLLOUT)) {
    *events |= DP_WRITE_READY;
  }
}

}  // namespace base

#endif  // LINUX
