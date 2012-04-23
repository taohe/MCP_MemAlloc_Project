#ifndef MCP_HTTP_SERVICE_HEADER
#define MCP_HTTP_SERVICE_HEADER

#include "io_service.hpp"
#include "http_connection.hpp"

namespace http {

using base::AcceptCallback;
using base::IOService;

// The HTTPService allows HTTP connections to be formed, both on the
// client and on the server sides, and links them to a given
// IOService.
//
// There are some HTTP documents that perform special tasks.  The
// IOService can be stopped by issuing a '/quit' HTTP GET request. The
// '/stat' documet would return a statistics page for the underlying
// IOService. Any other request attempt would result in trying to read
// a file from disk with that document name.
class HTTPService {
public:
  // Starts a listening HTTP service at 'port'. A HTTP service
  // instance always has a server running.
  HTTPService(int port, IOService* io_service);

  // Stops the listening server. Assumes no AcceptCallbacks are
  // pending execution (ie, the io_service is stopped).
  ~HTTPService();

  // Server Side

  // TODO
  //   + Register Request Handlers (see HTTPServerConnection::handleRequest)

  // Client side

  // Tries to connect to 'host:port' and issues 'cb' with the
  // resulting attempt. ConnectCallback takes an HTTPConnection as
  // parameter.
  void asyncConnect(const string& host, int port, ConnectCallback* cb);

  // Synchronous dual of asyncConnect. The ownership of
  // HTTPClientConnection is transfered to the caller.
  void connect(const string& host, int port, HTTPClientConnection** conn);

private:
  IOService*      io_service_;  // not owned here

  // Starts the server-side of a new HTTP connection established on
  // socket 'client_fd'.
  void acceptConnection(int client_fd);

  // Completion call used in synchronous connection.
  void connectInternal(Notification* n,
                       HTTPClientConnection** user_conn,
                       HTTPClientConnection* new_conn);
};

}  // namespace http

#endif // MCP_HTTP_SERVICE_HEADER
