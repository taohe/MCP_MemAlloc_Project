#include <arpa/inet.h>
#include <cstring>      // strerror
#include <errno.h>      // errno
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>     // read, write, close

#include <iostream>

#include "callback.hpp"
#include "connection.hpp"
#include "io_manager.hpp"
#include "io_service.hpp"
#include "logging.hpp"

namespace base {

Connection::Connection(IOService* io_service, int client_fd)
  : writing_(false),
    client_fd_(client_fd),
    closed_(false),
    io_service_(io_service),
    in_error_(false),
    refs_(0) {

  // Puts the Descriptor in read/write mode. Descriptor takes
  // ownership of the upcalls.
  Callback<void>* readUpCall = makeCallableMany(&Connection::doRead, this);
  Callback<void>* writeUpCall = makeCallableMany(&Connection::doWrite, this);
  IOManager* io_manager = io_service_->io_manager();
  io_desc_ = io_manager->newDescriptor(client_fd_, readUpCall, writeUpCall);
}

Connection::Connection(IOService* io_service)
  : writing_(false),
    client_fd_(-1),
    closed_(true),
    io_service_(io_service),
    io_desc_(NULL),
    in_error_(false),
    refs_(0) {

  // The Descriptor 'io_desc_' will be put in connection mode in
  // startConnect(), if the connection doesn't complete
  // immediately. Otherwise, the descriptor will be set to wait for
  // connectin completion.
}

Connection::~Connection() {
  int res;
  if (client_fd_ > 0) {
    do {
      res = ::close(client_fd_);
    } while ((res < 0) && (errno == EINTR));
  }

  if (io_desc_) {
    IOManager* io_manager = io_service_->io_manager();
    io_manager->delDescriptor(io_desc_);
  }
}

void Connection::acquire() {
  ScopedLock l(&m_refs_);
  refs_++;
}

void Connection::release() {
  m_refs_.lock();
  int count = --refs_;
  m_refs_.unlock();

  if (count == 0) {
    delete this;
    return;
  }

  if (count < 0) {
    LOG(LogMessage::ERROR) << "Error in release" << client_fd_;
  }
}

void Connection::startConnect(const string& host, int port) {
  // Create a non-blocking socket.
  client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd_ < 0) {
    error_string_.append("Socket failed: ");
    error_string_.append(strerror(errno));
    in_error_ = true;
    return;
  }
  int flags = fcntl(client_fd_, F_GETFL, 0);
  fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK);

  // Prepare to and start connection to 'host:port'.
  struct sockaddr_in serv_addr;
  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr);
  int res = connect(client_fd_, (sockaddr*) &serv_addr, sizeof(serv_addr));

  // Regardless of the success on connecting, we expect the user's
  // 'connDone()' up-call to be issued. Therefore doConnect() is called
  // -- directly, if success or failure in connecting is detected
  // immediately, or indirectly, otherwise. The acquire() here is
  // matched by a release() in doConnect().
  acquire();
  if (res >= 0) {
    doConnect();

  } else if ((errno != EINPROGRESS) && (errno != EINTR)) {
    ::close(client_fd_);
    client_fd_ = -1;
    error_string_.append("Connect failed: ");
    error_string_.append(strerror(errno));
    in_error_ = true;
    doConnect();

  } else {
    Callback<void>* write_cb = makeCallableMany(&Connection::doConnect, this);
    IOManager* io_manager = io_service_->io_manager();
    io_desc_ = io_manager->newDescriptor(client_fd_, NULL, write_cb);
    io_desc_->writeWhenReady();
  }
}

void Connection::doConnect() {
  // Check for errors in the connection process -- if they haven't
  // already been detected.
  if (! in_error_) {
    int error;
    socklen_t len = sizeof(error);
    getsockopt(client_fd_, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
      error_string_.append("Connect failed");
      error_string_.append(strerror(error));
      in_error_ = true;

    } else {

      // Put the underlying descriptor in a state to accept reads and
      // writes.
      closed_ = false;
      Callback<void>* read_cb = makeCallableMany(&Connection::doRead, this);
      Callback<void>* write_cb = makeCallableMany(&Connection::doWrite, this);
      io_desc_->setUpCalls(read_cb, write_cb);
    }
  }

  // Run the sub-class's code for connection handling.
  connDone();

  // This release() matches the acquire on the startConnect() call.
  release();
}

void Connection::startRead() {
  acquire();
  io_desc_->readWhenReady();
}

static int socketRead(int fd, char* buf, size_t size) {
  int res;
  do {
    res = read(fd, buf, size);
  } while ((res < 0) && (errno == EINTR));
  return res;
}

void Connection::doRead() {
  while (true) {
    in_.reserve(1024);
    int bytes_read = socketRead(client_fd_, in_.writePtr(), in_.writeSize());
    if (bytes_read > 0) {
      in_.advance(bytes_read);
    }

    if ((bytes_read < 0) && (errno == EAGAIN)) {
      acquire();
      io_desc_->readWhenReady();
      break;

    } else if (bytes_read < 0) {
      LOG(LogMessage::WARNING)
        << "Error on read (" << client_fd_ << "): " << strerror(errno);
      break;

    } else if (bytes_read == 0) {
      // The socket was closed.
      break;

    } else if (!readDone()) {
      LOG(LogMessage::WARNING)
        << "Error procesing read (" << client_fd_ << ")";
      break;
    }

    // Continue issuing reads.
  }

  // This release matches the acquire done when scheduling the
  // startRead() call.
  release();
}

void Connection::startWrite() {
  {
    ScopedLock l(&m_write_);
    if (writing_) {
      return;
    }
    writing_ = true;
  }

  acquire();
  doWrite();
}

static int socketWrite(int fd, const char* buf, size_t size) {
  int res;
  do {
    res = write(fd, buf, size);
  } while ((res < 0) && (errno == EINTR));
  return res;
}

void Connection::doWrite() {
  while (true) {
    // Note that size here is the contiguous part of it It may very
    // well be more data on the buffer that is not contiguous and thus
    // would not appear in here.
    m_write_.lock();
    int size = out_.readSize();
    const char* data = out_.readPtr();
    m_write_.unlock();

    int bytes_written = socketWrite(client_fd_, data, size);

    {
      ScopedLock l(&m_write_);

      if ((bytes_written < 0) && (errno == EAGAIN)) {
        acquire();
        io_desc_->writeWhenReady();
        break;

      } else if (bytes_written < 0) {
        // LOG(LogMessage::ERROR) << "Error on write " << strerror(errno);
        std::cout << "Error on write " << strerror(errno) << std::endl;
        break;

      } else if (bytes_written == 0) {
        // LOG(Logmessage::NORMAL) << "Closing on write " << client_fd_;
        std::cout << "Closing on write " << client_fd_ << std::endl;
        break;

      }

      out_.consume(bytes_written);
      if ((size == bytes_written) && (out_.readSize() == 0)) {
        writing_ = false;
        break;
      }

      // Continue writing remaining data.

    }
  }

  // This release matched the acquire that scheduled the doWrite()
  release();
}

} // namespace base
