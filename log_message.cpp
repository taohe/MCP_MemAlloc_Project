#include <iomanip>
#include <stdlib.h> // exit()

#include "log_message.hpp"
#include "log_writer.hpp"

bool MCP_BASE_exit_on_fatal = true;
bool MCP_BASE_has_fatal_message = false;

namespace base {

LogMessage::LogMessage(const char* file, const int line, Severity severity)
  : severity_(severity) {
  char labels[MAX_SEVERITY] = { ' ', 'W', 'E', 'F' };
  msg_stream_ << labels[severity_] << " " << file << ":" << line << " ";
}

LogMessage::~LogMessage() {
  flush();
  if (severity_ == FATAL) {
    if (MCP_BASE_exit_on_fatal) {
      exit(1);
    } else {
      MCP_BASE_has_fatal_message = true;
    }
  }
}

void LogMessage::flush() {
  msg_stream_ << std::endl;
  LogWriter::instance()->write(msg_stream_.str());
}

ostream& LogMessage::stream() {
  return msg_stream_;
}

}  // namespcae base
