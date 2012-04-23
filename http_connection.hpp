#ifndef MCP_HTTP_CONNECTION_HEADER
#define MCP_HTTP_CONNECTION_HEADER

#include <queue>
#include <string>

#include "callback.hpp"
#include "connection.hpp"
#include "http_request.hpp"
#include "io_service.hpp"
#include "lock.hpp"

namespace http {

using std::queue;
using std::string;
using base::IOService;
using base::Mutex;
using base::Notification;

class Response;

// This class handles the server side of an HTTP connection -- a very
// rudimentary set of HTTP, as of now. It can handle a GET request for
// a given document and it can react to a special 'quit' request. The
// latter will shutdown the HTTP server this connection belongs to.
//
// Thread safety:
//
//   This class is not thread safe. It assumes that readDone would not
//   be called concurrently. That's the expected behavior from
//   Connection. Still, readDone may want to write to its out_ buffer
//   while that buffer is being read from by the io_manager machinery
//   (e.g., reading data to be sent out). So your code should only
//   touch that with m_write_ locked.
//

class HTTPServerConnection : public base::Connection {
public:

  // Takes the IOService that this connection belongs to.  This enables
  // terminating it if the special 'quit' request arrives
  HTTPServerConnection(IOService* service, int client_fd);

private:
  Request    request_;

  // base::Connection is ref counted. Use release() to
  // delete. Normally, you won't need to because the io_manager will
  // do that for you.
  virtual ~HTTPServerConnection() {}

  // Parses as many requests as there are in the input buffer and
  // generates responses for each of them.
  virtual bool readDone();

  // Processes 'request', be it an internal command such as '/quit' or
  // '/stats' and returns true if more request should be accepted in
  // this connection.
  //
  // TODO
  // The command handlers are hardcoded here. Ideally, we should
  // register them with the server itself and we'd look them up here.
  // see TODO in Server class
  bool handleRequest(Request* request);

  // Non-copyable, non-assignable
  HTTPServerConnection(HTTPServerConnection&);
  HTTPServerConnection& operator=(HTTPServerConnection&);
};

// This class handles the client-side of an HTTP connection. It is
// built so that it can connect to an HTTP server and send requests
// and get responses in a non-blocking way.
//
// Thread Safety:
//
//   The class is safe in that sendRequests() may be issued
//   concurrently. connect() should be issued only once, though.
class HTTPClientConnection;
typedef base::Callback<void, HTTPClientConnection*> ConnectCallback;
typedef base::Callback<void, Response*> ResponseCallback;

class HTTPClientConnection : public base::Connection {
public:
  // Copies 'request' into the connection output buffer and schedules
  // a write. Enqueues 'cb' for when a reponse arrives.
  void asyncSend(Request* request, ResponseCallback* cb);

  // Issues 'request' and blocks until 'response' arrives. This is the
  // synchronous version of send.
  void send(Request* request, Response** response);

private:
  friend class HTTPService;

  // The connect callback is always issued, independently of the
  // result of the connect itself. 'connect_cb_' would thus self
  // delete if it is a once only callback.
  ConnectCallback*         connect_cb_;

  Mutex                    m_response_;    // protects the queue below
  queue<ResponseCallback*> response_cbs_;  // owned here

  // Client connections take the IOService they belong to but unlike
  // Server connections they only use the server's io_manager. We take
  // the Server here, too, so that both client and server connections
  // would use the same server stopping procedure.
  explicit HTTPClientConnection(IOService* service);

  // base::Connection is reference counted so the destructor shouldn't
  // be issued directly. The connection would get deleted if
  // 'connDone()' below doesn't start reading or if 'readDone()'
  // returns false (to stop reading).
  virtual ~HTTPClientConnection() { }

  // Tries to connect with a server sitting on 'host:port' and issue
  // 'cb' with the resulting attempt.
  void connect(const string& host, int port, ConnectCallback* cb);

  // If the connect request went through, start reading from the
  // connection. In any case, issue the callback that was registered
  // in the 'connect()' call.
  virtual void connDone();

  // Parses as many responses as there are in the input buffer for
  // this connection and issues one callback per parsed response.
  virtual bool readDone();
  bool handleResponse(Response* response);

  // Completion call used in sync receive().
  void receiveInternal(Notification* n,
                       Response** user_response,
                       Response* new_response);

  // Non-copyable, non-assignable
  HTTPClientConnection(HTTPClientConnection&);
  HTTPClientConnection& operator=(HTTPClientConnection&);
};

}  // namespace htpp

#endif // MCP_HTTP_CONNECTION_HEADER
