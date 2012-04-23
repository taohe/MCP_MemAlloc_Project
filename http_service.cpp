#include <errno.h>
#include <string.h>  // strerror

#include "callback.hpp"
#include "http_service.hpp"
#include "http_connection.hpp"
#include "logging.hpp"

namespace http {

using base::Callback;
using base::makeCallableMany;
using base::makeCallableOnce;

HTTPService::HTTPService(int port, IOService* io_service)
  : io_service_(io_service) {
  AcceptCallback* cb = makeCallableMany(&HTTPService::acceptConnection, this);
  io_service_->registerAcceptor(port, cb /* ownership xfer */);
}

HTTPService::~HTTPService() {
}

void HTTPService::acceptConnection(int client_fd) {
  if (io_service_->stopped()) {
    return;
  }

  if (client_fd < 0) {
    LOG(LogMessage::ERROR) << "Error accepting: " << strerror(errno);
    io_service_->stop();
    return;
  }

  // The client will be destroyed if the peer closes the socket(). If
  // the server is stopped, all the connections leak. But the
  // sockets will be closed by the process termination anyway.
  new HTTPServerConnection(io_service_, client_fd);
}

void HTTPService::asyncConnect(const string& host,
                               int port,
                               ConnectCallback* cb) {
  if (io_service_->stopped()) {
    return;
  }

  HTTPClientConnection* conn = new HTTPClientConnection(io_service_);
  conn->connect(host, port, cb);
}

void HTTPService::connect(const string& host,
                          int port,
                          HTTPClientConnection** conn) {
  Notification n;
  ConnectCallback* cb = makeCallableOnce(&HTTPService::connectInternal,
                                         this,
                                         &n,
                                         conn);
  asyncConnect(host, port, cb);
  n.wait();
}

void HTTPService::connectInternal(Notification* n,
                                  HTTPClientConnection** user_conn,
                                  HTTPClientConnection* new_conn) {
  *user_conn = new_conn;
  n->notify();
}

} // namespace http
