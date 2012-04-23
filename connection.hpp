#ifndef MCP_BASE_CONNECTION_HEADER
#define MCP_BASE_CONNECTION_HEADER

#include <string>

#include "buffer.hpp"
#include "lock.hpp"

namespace base {

using std::string;

// This class interacts directly with the io_manager so to support
// asynchronous communication protocols. To implement a protocol's
// format (requests and responses layouts and semantics), a sub-class
// provides callbacks that are issued whe events such as connecting,
// reading, or writing complete in the underlying descriptor.
//
// A Connection carries two streaming buffers, one for input and
// another for output. The connection keeps reading the underlying
// descriptor for as long as it doesn't block, pushing the data to the
// input buffer, and calling the appropriate method on a
// sub-class. The writing part of the connection takes whatever is in
// the output stream and flushes it in the underlying descriptor, for
// as long as it has something to read from.
//
// Connections are reference counted. Never destroy a connection
// directly. As long as your protocol keeps reading/writing, a
// connection will be active.
//
// Thread safety:
//
//   + The public methods are thread safe and the reading and writing
//     portions of a Connection will probably run concurrently.
//
//   + There's only one outstanding read operation. While the
//     readDone() is running, all reading on the corresponding socket
//     stops. Conversely, while socket reading is being done, there's
//     no read callbacks executing.
//
//   + Writes to the output stream and from there to the socket may
//     occur concurrently. So the output buffer needs to be protected
//     the m_write_ lock.
//
// Example Usage:
//
//   class MyServerConnection : public base::Connection {
//   public:
//     MyServerConnection(IOService* io_service, int client fd)
//       : Connection(io_manager, fd) {
//       startRead();
//     }
//
//     virtual bool readDone() {
//       while (true) {
//         consume data available at in_ buffer;
//
//         generate response
//         m_write_.lock();
//         dump response data into the out_ buffer;
//         m_write_.unlock();
//
//         startWrite();
//       }
//     }
//   };
//
//   MyServerConnection* conn = new MyServerConnection(io_mgr, fd);
//

class Descriptor;
class IOService;

class Connection {
public:
  // Connection status and last error.
  bool ok() { return ! in_error_; }
  bool closed() { return closed_; }
  string errorString() { return error_string_; }

  // This class is reference counted. A newly constructed object will
  // need to issue the acquire() unless the object holder issues a
  // startRead(), startWrite(), or a startConnect() against the object
  // --which is usually the case.
  void acquire();
  void release();

  // accessors

  IOService* io_service() const { return io_service_; }

protected:
  // There should be at most one thread on the read portion of the
  // class. Therefore, there is no need to protect the input buffer.

  Buffer          in_;

  // There is concurrency between a thread appending to the output
  // buffer and one reading from it (to send its contents through the
  // connection). m_write_'s job is to synchronized that.

  Mutex           m_write_;         // protects state below
  bool            writing_;         // has a pending/ongoing write request
  Buffer          out_;

  // Passive (server-side) connection constructor. The ref counter is
  // not incremented here as a sub-class passive constructor would
  // probably issue a startRead() right away.
  Connection(IOService* io_service, int client_fd);

  // Active (client-side) connection constructor. Builds a connection
  // and prepares it to be connected. The ref counter is not
  // incremented here, since a sub-class would want to issue a
  // 'startConnect()' to trigger the connection process right away.
  explicit Connection(IOService* io_service);

  // Issued when refs_ become zero. Do not destroy the object directly
  // through this destructor.
  virtual ~Connection();

  // Starts the process of connecting to 'host:port'. When the connect
  // process completes -- succesfully or not -- the sub-class
  // 'connDone()' call is issued (possibly within 'startConnect()',
  // depending on the speed of the connect completion).
  //
  // This call increments the reference count for the time it takes to
  // both wait for the connect and to issue 'connDone()'. In other
  // words, if connDone() doesn't do anything that maintains the
  // connection open, the ref counter may be zeroed and the connection
  // cleaned after connDone() returns. This may be appropriate if an
  // error happened in the connect processs.
  //
  // Note: Can only be issued once. To attempt a connection again, use
  // a new instance.
  void startConnect(const string& host, int port);

  // Starts reading continuously from this connection, populating the
  // 'in_' buffer with the results and issuing the 'readDone()' then.
  // The read is "continuous" as in the return of the 'readDone()'
  // determines if the process of reading ther underlying socket
  // should continue or not. As so, this call should be issued only
  // once.
  //
  // This call increments the reference count for the time it takes to
  // both wait for the connection read and to issue 'readDone()'. The
  // counter is decremented only when reading process is aborted.
  void startRead();

  // If there isn't an ongoing write yet, starts one. An ongoing write
  // will flush to the underlying socket all the contents of 'out_' at
  // that juncture. This call should be issued every time 'out_' was
  // written to.
  //
  // Tha call increments the reference count for the time it takes to
  // wait for the underlying socket to be ready until the writing the
  // data in 'out_' to it.
  void startWrite();

  // Closes the underlying file descriptor.
  void close() {
    ::close(client_fd_);
    closed_ = true;
  }

private:
  int             client_fd_;       // socket to peer
  bool            closed_;          // fd in use?
  IOService*      io_service_;      // not owned by this
  Descriptor*     io_desc_;         // owned by this
  bool            in_error_;        // last op failed?
  string          error_string_;    // last error description

  Mutex           m_refs_;          // protects refs_
  int             refs_;            // reference counting state

  // Internal read helper called when 'startRead()' can effectively
  // run. Decrements reference count at the end.
  void doRead();

  // Called when there is data in the input buffer to be
  // read. Subclasses need to implement this to handle the parsing and
  // processing of the packet. Returns true if the read was successful
  // or false otherwise. Note that a successful read may or may not
  // 'consume()' the data in the buffer. A return value of false will
  // cause the connection to close.
  virtual bool readDone() { return false; }

  // Internal connect helper called when 'startConnect()' can
  // effectivelly run. Decrements reference count at the end.
  void doConnect();

  // Called when the connection process initiated in startConnect()
  // finshes. Subclasses need to implement this so they know when the
  // writing/reading to the new connection starts being possible. The
  // implementation must check if the connection went trough or not.
  virtual void connDone() {}

  // Writes all data available in the 'out_' Buffer into
  // 'client_fd'.
  void doWrite();

  // Non-copyable, non-assignable
  Connection(Connection&);
  Connection& operator=(Connection&);
};

} // namespace base

#endif // MCP_BASE_CONNECTION_HEADER
