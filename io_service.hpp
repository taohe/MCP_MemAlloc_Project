#ifndef MCP_BASE_IO_SERVICE_HEADER
#define MCP_BASE_IO_SERVICE_HEADER

#include <vector>

#include "callback.hpp"
#include "request_stats.hpp"
#include "lock.hpp"

namespace base {

using std::vector;

class Acceptor;
class IOManager;

// AcceptCallback is an upcall that takes a socket endpoint. The
// endpoint is created as a result of a connect request to the
// service. See IOService::registerAcceptor() for details.
typedef Callback<void, int> AcceptCallback;

// An IOService connects together basic machinery to build higher
// level networking protocols. It allow several protocols to share a
// same event loop (io_manager). The protocols' message handling can
// start and stop in unison. Stats reporting can also be done on the
// collective of protocols.
//
// Internally, the IOService uses one dedicated thread for pooling for
// ready sockets (including listening ones) and a thread pool to serve
// the callbacks associated with these sockets.
//
// Thread Safety:
//
//   The class is thread safe. In particular, it is safe to call
//   stop() from any thread. It is also safe to call it more than
//   once, even if from different threads.
//
class IOService {
public:
  explicit IOService(int num_workers = 1);

  // Destroys an IOService that was start()-ed or not.
  //
  // NOTE: The destructor must never be issued if start() hasn't
  // returned.
  ~IOService();

  // Installs 'cb' as the callback issued when connect requests are
  // received in 'port'. AcceptCallback takes the socket endpoint that
  // the service should use to talk to one client. The service takes
  // ownership of the callback.
  //
  // Note: Acceptors must be installed prior to starting the Service.
  void registerAcceptor(int port, AcceptCallback* cb);

  // Blocks the calling thread and starts accepting connections and
  // handling their requests. This call is guaranteed only to return
  // after stop() finished running.
  void start();

  // Carefully tears down a running, start()-ed IOService by stopping its
  // worker pool and pooling thread.
  void stop();

  // Returns true if stop() was issued.
  bool stopped() const;

  // accessors

  IOManager* io_manager() { return io_manager_; }
  RequestStats* stats() { return &stats_; }

private:
  typedef vector<Acceptor*> Acceptors;

  // request serving machinery
  IOManager*        io_manager_;     // owned here
  Acceptors         acceptors_;      // owned here
  RequestStats      stats_;

  // stop-state maintenance
  mutable Mutex     m_stop_;         // protects variables below
  ConditionVar      cv_stopped_;     // cond: stopped_ == true
  bool              stop_requested_; // stop() issued already
  bool              stopped_;        // stop() completed

  // Non-copyable, non-assignable
  IOService(IOService&);
  IOService& operator==(IOService&);
};

}  // namespace base

#endif  // MCP_BASE_IO_SERVICE_HEADER
