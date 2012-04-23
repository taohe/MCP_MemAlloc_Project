#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>     // perror
#include <stdlib.h>    // exit
#include <strings.h>   // bzero
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "acceptor.hpp"
#include "callback.hpp"
#include "io_manager.hpp"

namespace base {

Acceptor::Acceptor(IOManager* io_manager, int port, AcceptCallback* cb)
  : io_manager_(io_manager),
    io_descr_(NULL),
    accept_cb_(cb) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    perror("Socket failed");
    exit(1);
  }

  struct sockaddr_in server;
  bzero((char*)&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  if (::bind(listen_fd_, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Bind failed");
    exit(1);
  }

  if (listen(listen_fd_, 20) < 0) {
    perror("Listen failed");
    exit(1);
  }

  // Descriptor takes ownership of the upcalls.
  Callback<void>* readUpCall = makeCallableMany(&Acceptor::doAccept, this);
  Callback<void>* writeUpCall = makeCallableMany(&Acceptor::noOp, this);
  io_descr_ = io_manager_->newDescriptor(listen_fd_, readUpCall, writeUpCall);
}

Acceptor::~Acceptor() {
  if (io_descr_ != NULL) {
    close();
  }

  // We require that the io_manager be stopped here. So we can safely
  // reclaim the accept callback.
  delete accept_cb_;
  accept_cb_ = NULL;
}

void Acceptor::close() {
  // By closing the descriptor, it wil be excluded from the epoll
  // (keque) event set automatically. We need to dispose of the
  // Descriptor here, while the io_manager is alive (and lazily
  // cleaning deleted Descriptors.)
  ::close(listen_fd_);
  if (io_descr_) io_manager_->delDescriptor(io_descr_);
  io_descr_ = NULL;

  // Note that we do not dispose of the accept_cb_ here, for the
  // same reason above: we asssume the io_manger is sitll running
  // and therefore there might be some accept_cb_ outstanding in
  // the worker queue.
}

void Acceptor::startAccept() {
  io_descr_->readWhenReady();
}

static int socketAccept(int fd, struct sockaddr* addr, socklen_t* len) {
  int res;
  do {
    res = accept(fd, addr, len);
  } while ((res < 0) && ((errno == EINTR) || (errno == ECONNABORTED)));
  return res;
}

void Acceptor::doAccept() {
  while (true) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int fd = socketAccept(listen_fd_, (struct sockaddr*) &client, &len);

    if ((fd < 0) && (errno == EAGAIN)) {
      io_descr_->readWhenReady();
      break;
    }

    if (accept_cb_ != NULL) {
      (*accept_cb_)(fd);
    }
  }
}

void Acceptor::noOp() {
  // used in the io_descr_ write upcall
}

} // namespace base

