#ifndef MCP_BASE_IO_MANAGER_HEADER
#define MCP_BASE_IO_MANAGER_HEADER

#include <map>
#include <pthread.h>
#include <queue>
#include <vector>

#include "callback.hpp"
#include "lock.hpp"
#include "thread_pool_fast.hpp"
#include "ticks_clock.hpp"

namespace base {

using std::multimap;
using std::queue;
using std::vector;

using base::Callback;
using base::TicksClock;

class Descriptor;
class DescriptorPoller;

// We can register a socket with the IOManager and ask it to issue a
// notifying callback whenever that socket is ready to read or to
// write. The result of this arrangement is that our callback will
// only be issued at a time where we know reading/writing to a socket
// won't block the responsible thread.
//
// SOCKETS PASSED TO THIS CLASS MUST BE NON-BLOCKING
//
// Internally, the IOManager runs a 'polling' thread (epoll) to scan
// through the sockets continuously. When a socket is found to be
// ready, the IOManager runs the corresponding callback in a 'workers'
// thread pool.
//
//
// Thread Safety:
//
//   The class is thread-safe in that stop() will most definitely be
//   called on a different thread than poll() (because the latter
//   blocks). But the stopping procedure should use the care describe
//   in poll() and stop().
//
//
// Usage:
//
//   See the http::Server class.
//

class IOManager {
public:
  // Builds an IOManager instance backed by a thread pool with
  // 'num_workers' threads. The threads are dedicated for running the
  // upcall registered (see newDescriptor below).
  explicit IOManager(int num_workers);

  // The destructor requires stop() to complete before it can be
  // issued.
  ~IOManager();

  // Blocks the calling thread and starts polling for ready sockets on
  // it. Upon finding such sockets, their upcalls would be issued in a
  // non-determined thread in the worker pool. This call will return
  // only when stop() is issued.
  void poll();

  // Stops the polling thread and the workers thread pool, returning
  // only when all that machinery was tore down.
  //
  // NOTE:
  //   stop() breaks the polling loop and, so, it makes poll() return.
  //   However, there may be still callbacks pending that will be
  //   serverd before the stop() itself returns. If the poll() thread
  //   goes on to destruct the io_manager, the stop() here will
  //   segfault.
  void stop();

  // callback registration support

  // Starts polling for readiness of 'fd'. When it is ready to read,
  // issue 'rcb', and ditto for write and 'wrb'.
  Descriptor* newDescriptor(int fd, Callback<void>* rcb, Callback<void> *wrb);

  // Marks 'desc' as ready to be GC-ed and returns.
  void delDescriptor(Descriptor* desc);

  // Timed execution support

  // Schedules 'task' to be executed at least 'delay' seconds
  // (possibly fractional) from now.
  void addTimer(double delay, Callback<void>* task);

  // Schedules 'task' to be executed as soon as possible by one of the
  // io_manager's workers.
  void addTask(Callback<void>* task);

private:
  friend class Descriptor;

  DescriptorPoller* poller_;       // polling descriptor service
  pthread_t         poll_thread_;  // thread running epoll
  ThreadPoolFast*   worker_pool_;  // threads running upcalls, owned here

  // A descriptor that got closed will add itself to this
  // list. pollBody() periodically disposes of them. All GC activity
  // is protected by m_deleted_desc_.
  Mutex             m_deleted_desc_;
  Descriptor*       deleted_desc_; // head of deleted descriptors

  // All stopping state is protected by m_stop_.
  mutable Mutex     m_stop_;
  bool              stopped_;      // has stop been requested?
  bool              polling_;      // is polling still ongoing?
  ConditionVar      cv_polling_;   // signal polling stopped

  // Keeps the timestamps for the next alarms and their respective
  // callbacks. All access to the queue is protected by
  // m_timer_queue_.
  Mutex             m_timer_queue_;
  typedef multimap<TicksClock::Ticks, Callback<void>* > TimerQueue;
  TimerQueue        timer_queue_;

  // Loops through registered descriptors and issues the related
  // callback when ready. In between iterations, garbage collect
  // descriptors that are no longer in use.
  void pollBody();

  // Returns true if stop() was issue (but not necessarily completed).
  bool stopped() const;
};

// Descriptors are the interface to issuing request to the
// IOManager. Each Descriptor represents an open socket with the
// associated callbacks for when reads or writes can be issued that
// won't block.
//
// A Descriptor is created and disposed of via the IOManager.
//
// Note:
//
//   The underlying assumption here is that we'll be handling IO in
//   the socket in EDGE TRIGGERED MODE. Simply put, any read or write
//   to the socket should consume all data -- until a EAGAIN results
//   -- before asking the IOManager to check on the socket's readiness
//   again.
//
class Descriptor {
public:
  // If a socket can be read from, schedules the associated read
  // callback on the IOManager's thread pool and return. Otherwise, mark the
  // Descriptor as 'wants to read' and return.
  void readWhenReady();

  // Similar to readWhenReady() but for writes.
  void writeWhenReady();

  // Replaces both the read and write callbacks with 'read_cb' and
  // 'write_cb' respectively. Old callbacks, if any, are disposed and
  // ownership of 'read_cb' and 'write_cb' are transferred to this
  // Descriptor.
  void setUpCalls(Callback<void>* read_cb, Callback<void>* write_cb);

  // accessors

  int fd() const { return fd_; }

private:
  friend class IOManager;

  IOManager*      io_manager_;     // my manager, not owned by this
  int             fd_;             // underlying socket descriptor
  bool            closed_;         // was fd_ closed?

  // All callback state is protected by m_.
  Mutex           m_;
  Callback<void>* read_cb_;        // read upcall, owned here
  Callback<void>* write_cb_;       // write upcall, owned here
  bool            can_read_;       // can serve a read immediately
  bool            can_write_;      // ditto write
  bool            waiting_read_;   // a read wasr requested
  bool            waiting_write_;  // ditto write

  // List of descriptors that can be disposed.
  Descriptor*     next_;

  // io_manager's interface

  // Creates a descriptor for the socket 'fd' on 'io_manager' and
  // register 'read_cb' and 'write_cb' as the upcalls for when the
  // socket can be read and written to, respectively.
  Descriptor(IOManager* io_manager,
             int fd,
             Callback<void>* read_cb,
             Callback<void>* write_cb);
  ~Descriptor();

  // If the socket is on a 'wants to be read' mode, issues the read
  // callback on the io_manager's threadpool and return. Otherwise,
  // mark the socket as ready to read and return.
  void readIfWaiting();

  // Similar to readIfWaiting() but for writes.
  void writeIfWaiting();

  // accessors

  ThreadPoolFast* workerPool() { return io_manager_->worker_pool_; }
};

} // namespace base

#endif // MCP_BASE_IO_MANAGER_HEADER
