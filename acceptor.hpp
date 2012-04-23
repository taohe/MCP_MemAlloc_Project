#ifndef MCP_BASE_ACCEPTOR_HEADER
#define MCP_BASE_ACCEPTOR_HEADER

#include "io_service.hpp"
#include "lock.hpp"

namespace base {

class IOManager;
class Descriptor;

// The Acceptor handles a listening socket.  It uses an assigned
// io_manager so to never block. Upon forming a connection, the
// Acceptor issues a previously registered callback that takes the
// client socket as an argument.
//
// Thread-safety:
//
//   This class is *not* thread safe. In particular, '~Acceptor()'
//   should only be called if there's no ongoing 'doAccept()'
//   running. We depend on the underlying io_manager being stopped()
//   itself for that to be true.
//
//
// Usage:
//   // io_manager previously declared
//   Acceptor acceptor(&io_manager, 15001);
//   acceptor.startAccept(<callback>);
//
class Acceptor {
public:

  // Builds an acceptor that will use 'io_manager' to listen to 'port'
  // and Registers 'cb' with the acceptor and start taking connection
  // requests. The acceptor is not active before 'startAccept()' is
  // called.
  Acceptor(IOManager* io_manager, int port, AcceptCallback* cb);

  // NOTE
  //   this will get rid of AcceptCallback. So do NOT destruct if
  //   there might be pending callbacks.
  ~Acceptor();

  // Starts taking connection requests and issuing the AcceptCallback
  // once for each new connection attempt. If the connection was
  // formed successfully, a valid client socket descriptor would be
  // passed. Otherwise, -1 is passed as a descriptor.
  //
  // NOTE: must be called only once.
  void startAccept();

  // Unregisters the accept callback and stops accepting new
  // connections from this point on.
  //
  // NOTE
  //   This must be called before the io_manager itself was stopped.
  void close();

private:
  int             listen_fd_;
  IOManager*      io_manager_;  // not owned by this
  Descriptor*     io_descr_;    // not owned by this
  AcceptCallback* accept_cb_;   // owned by this

  // Internal socket accept loop.
  void doAccept();
  void noOp();

  // Non-copyable, non-assignable.
  Acceptor(const Acceptor&);
  Acceptor& operator=(const Acceptor&);
};

} // namespace base

#endif // MCP_BASE_ACCEPTOR_HEADER
