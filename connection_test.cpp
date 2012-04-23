#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>

#include <cstring>
#include <string>
#include <vector>

#include "buffer.hpp"
#include "acceptor.hpp"
#include "callback.hpp"
#include "connection.hpp"
#include "io_service.hpp"
#include "lock.hpp"
#include "logging.hpp"
#include "test_unit.hpp"
#include "thread.hpp"

namespace {

using base::AcceptCallback;
using base::Acceptor;
using base::Buffer;
using base::Callback;
using base::Connection;
using base::IOService;
using base::makeCallableMany;
using base::makeCallableOnce;
using base::Notification;

using std::string;
using std::vector;

// ****************************************
// Simple Synchronous Echo Server
//

enum { MAX_LINE = 80 };

class SyncServer {
public:
  explicit SyncServer(int port);
  ~SyncServer();

  void start();

private:
  int listen_fd_;
  int conn_fd_;
};

SyncServer::SyncServer(int port) : conn_fd_(-1) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int opt = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  ::bind(listen_fd_, (struct sockaddr *)&addr, sizeof(addr));

  listen(listen_fd_, 20);
}

SyncServer::~SyncServer() {
  close(listen_fd_);
}

void SyncServer::start() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  conn_fd_ = accept(listen_fd_, (struct sockaddr *)&addr, &len);

  int bytes = 0;
  for (;;) {
    char buffer[MAX_LINE];

    bytes = read(conn_fd_, buffer, MAX_LINE);
    if (bytes <= 0) {
      break;
    }

    if (strncmp(buffer,"quit", 4) == 0) {
      break;
    }

    bytes = write(conn_fd_, buffer, bytes);
    if (bytes <= 0) {
      break;
    }
  }

  close(conn_fd_);
  conn_fd_ = -1;
}

// ****************************************
// Simple Synchronouse Echo Client
//

class SyncClient {
public:
  SyncClient(const string& host, const string& port);
  ~SyncClient();

  bool sendMsg(const string& msg);
  bool recvMsg(string* msg);
  void close();

private:
  int conn_fd_;
};

SyncClient::SyncClient(const string& host, const string& port) {
  struct addrinfo *result;
  struct addrinfo *rp;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags= AI_NUMERICSERV;
  getaddrinfo(host.c_str(), port.c_str(), &hints, &result);

  for (rp = result; rp != NULL; rp = rp->ai_next) {

    conn_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (conn_fd_ == -1) {
      continue;
    }

    if (connect(conn_fd_, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    ::close(conn_fd_);
  }

  if (rp == NULL) {
    LOG(LogMessage::FATAL) << " couldn't connect";
  }

  freeaddrinfo(result);
}

SyncClient::~SyncClient() {
  this->close();
}

void SyncClient::close() {
  if (conn_fd_ != -1) {
    ::close(conn_fd_);
  }
}

bool SyncClient::sendMsg(const string& msg) {
  int bytes = write(conn_fd_, msg.c_str(), msg.size());
  if (bytes == -1) {
    LOG(LogMessage::ERROR) << strerror(errno);
    return false;
  }

  return true;
}

bool SyncClient::recvMsg(string* msg) {
  char buff[MAX_LINE];
  int bytes = read(conn_fd_, buff, MAX_LINE);
  if (bytes == -1) {
    std::cout << "ERROR: " << strerror(errno) << ": " << conn_fd_;
    LOG(LogMessage::ERROR) << strerror(errno);
    return false;
  }

  msg->append(buff, bytes);
  return true;
}


// ****************************************
// Declaration of an Echo client-server-service
//

class EchoServerConnection : public Connection {
public:
  EchoServerConnection(IOService* service, int conn_fd);

private:
  // Connection is ref counted.
  virtual ~EchoServerConnection();

  virtual bool readDone();
};

class EchoClientConnection : public Connection {
public:
  explicit EchoClientConnection(IOService* service);

  void connect(const char* host, int port);
  void sendMsg(const string& msg);
  void recvMsg(string* msg);
  void disconnect();

private:
  Notification connected_;
  Notification received_;
  Notification closed_;

  // Connection is ref counted.
  virtual ~EchoClientConnection();

  virtual void connDone();
  virtual bool readDone();
};

class EchoService {
public:
  EchoService(IOService* io_service);
  ~EchoService() { }

  void accept(int conn_fd_);
  void connect(const char* host, int port, EchoClientConnection** conn);
  void disconnect(EchoClientConnection* conn);

private:
  IOService* io_service_;  // not owned here
};

// ****************************************
// Echo Server Side
//

EchoServerConnection::EchoServerConnection(IOService* service, int conn_fd)
  : Connection(service, conn_fd) {
  startRead();
}

EchoServerConnection::~EchoServerConnection() {
}

bool EchoServerConnection::readDone() {
  string in_string(in_.readPtr(), in_.readSize());

  m_write_.lock();
  out_.write(in_string.c_str());
  m_write_.unlock();
  in_.consume(in_string.size());

  startWrite();
  return true;
}

// ****************************************
// Echo Client Side
//

EchoClientConnection::EchoClientConnection(IOService* service)
  : Connection(service) {
}

EchoClientConnection::~EchoClientConnection() {
}

void EchoClientConnection::connect(const char* host, int port) {
  startConnect(host, port);
  connected_.wait();
}

void EchoClientConnection::connDone() {
  startRead();
  connected_.notify();
}

void EchoClientConnection::sendMsg(const string& msg) {
  m_write_.lock();
  out_.write(msg.c_str());
  m_write_.unlock();

  startWrite();
}

void EchoClientConnection::recvMsg(string* msg) {
  received_.wait();

  const size_t len = in_.readSize();
  msg->append(in_.readPtr(), len);
  in_.consume(len);

  received_.reset();
}

bool EchoClientConnection::readDone() {
  received_.notify();
  if (closed()) {
    closed_.notify();
  }
  return true;
}

void EchoClientConnection::disconnect() {
  Connection::close();
}

// ****************************************
// Service class
//

EchoService::EchoService(IOService* io_service) : io_service_(io_service) { }

void EchoService::accept(int conn_fd) {
  if (conn_fd < 0) {
    LOG(LogMessage::ERROR) << "Error accepting: " << strerror(errno);
    return;
  }

  new EchoServerConnection(io_service_, conn_fd);
}

void EchoService::connect(const char* host,
                          int port,
                          EchoClientConnection** conn) {
  *conn = new EchoClientConnection(io_service_);
  (*conn)->connect(host, port);
}

void EchoService::disconnect(EchoClientConnection* conn) {
  conn->disconnect();
}

// ****************************************
// Test cases
//

TEST(Echo, SyncClientSyncServer) {
  SyncServer server(15001);
  Callback<void>* body = makeCallableOnce(&SyncServer::start, &server);
  pthread_t tid = base::makeThread(body);

  SyncClient client("127.0.0.1", "15001");
  const string out_string = "hello";
  EXPECT_TRUE(client.sendMsg(out_string));
  string in_string;
  EXPECT_TRUE(client.recvMsg(&in_string));
  EXPECT_EQ(in_string, out_string);

  client.sendMsg("quit");
  pthread_join(tid, NULL);
}

TEST(Echo, SyncClientAsyncServer) {
  IOService io_service(1 /* one worker */);
  EchoService echo_service(&io_service);
  AcceptCallback* cb = makeCallableMany(&EchoService::accept, &echo_service);
  io_service.registerAcceptor(15001, cb);
  Callback<void>* body = makeCallableOnce(&IOService::start, &io_service);
  pthread_t tid = base::makeThread(body);

  SyncClient client("127.0.0.1", "15001");
  const string out_string = "hello";
  string in_string;
  EXPECT_TRUE(client.sendMsg(out_string));
  EXPECT_TRUE(client.recvMsg(&in_string));
  EXPECT_EQ(in_string, out_string);
  client.close();

  io_service.stop();
  pthread_join(tid, NULL);
}

TEST(Echo, AsyncClientSyncServer) {
  SyncServer server(15001);
  Callback<void>* server_body = makeCallableOnce(&SyncServer::start, &server);
  pthread_t server_tid = base::makeThread(server_body);

  // We spin a service but only for the clients' benefit.
  IOService io_service(1 /* one worker */);
  EchoService echo_service(&io_service);
  Callback<void>* body = makeCallableOnce(&IOService::start, &io_service);
  pthread_t client_tid = base::makeThread(body);

  EchoClientConnection* client;
  echo_service.connect("127.0.0.1", 15001, &client);
  EXPECT_TRUE(client->ok());

  const string out_string = "hello";
  client->sendMsg(out_string);
  string in_string;
  client->recvMsg(&in_string);
  EXPECT_EQ(in_string, out_string);

  client->sendMsg("quit");
  pthread_join(server_tid, NULL);
  io_service.stop();
  pthread_join(client_tid, NULL);
}

TEST(Echo, AsyncClientAsyncServer) {
  IOService io_service(1 /* one worker */);
  EchoService echo_service(&io_service);
  AcceptCallback* cb = makeCallableMany(&EchoService::accept, &echo_service);
  io_service.registerAcceptor(15001, cb);
  Callback<void>* body = makeCallableOnce(&IOService::start, &io_service);
  pthread_t tid = base::makeThread(body);

  EchoClientConnection* client;
  echo_service.connect("127.0.0.1", 15001, &client);
  EXPECT_TRUE(client->ok());

  string out_string= "hello";
  client->sendMsg(out_string);
  string in_string;
  client->recvMsg(&in_string);
  EXPECT_EQ(in_string, out_string);

  out_string = "world";
  client->sendMsg(out_string);
  in_string.clear();
  client->recvMsg(&in_string);
  EXPECT_EQ(in_string, out_string);

  echo_service.disconnect(client);

  io_service.stop();
  pthread_join(tid, NULL);
}

TEST(Echo, WrongPort) {
  IOService io_service(1 /* one worker */);
  EchoService echo_service(&io_service);
  AcceptCallback* cb = makeCallableMany(&EchoService::accept, &echo_service);
  io_service.registerAcceptor(15001, cb);
  Callback<void>* body = makeCallableOnce(&IOService::start, &io_service);
  pthread_t tid = base::makeThread(body);

  EchoClientConnection* client;
  echo_service.connect("127.0.0.1", 15999 /* wrong port */, &client);
  EXPECT_FALSE(client->ok());

  echo_service.disconnect(client);

  io_service.stop();
  pthread_join(tid, NULL);
}

// TODO
//   Test server cleanup of connections.
//   Similar to asycncliensyncserver but here a connection should be
//   left open. this tests if the server leaks active connections when
//   shuting down


}  // unnamed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
