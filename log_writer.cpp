#include "log_writer.hpp"
#include "lock.hpp"

namespace base {

// LogWriter static state
pthread_once_t LogWriter::init_control_ = PTHREAD_ONCE_INIT;
LogWriter*     LogWriter::instance_ = NULL;

LogWriter::LogWriter() : log_file_(NULL) {}

LogWriter::~LogWriter() {
  fclose(log_file_);
}

void LogWriter::init() {
  instance_ = new LogWriter;
}

LogWriter* LogWriter::instance() {
  pthread_once(&init_control_, &LogWriter::init);
  return instance_;
}

bool LogWriter::createFile() {
  log_file_ = fopen("log.txt", "w");
  if (log_file_ == NULL) {
    perror("Could not create a log file");
    return false;
  }
  return true;
}

void LogWriter::write(const string& msg) {
  ScopedLock l(&m_);

  if ((log_file_ == NULL) && !createFile()) {
    // If the log file is not there, there is no way to log a
    // message. We'll try to create a new one in the next write() in
    // the hopes that this is a transient error.
    return;
  }
  fwrite(msg.c_str(), 1, msg.size(), log_file_);
}

}  // namespace base
