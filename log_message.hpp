#ifndef MCP_BASE_LOG_MESSAGE_HEADER
#define MCP_BASE_LOG_MESSAGE_HEADER

#include <iostream>
#include <sstream>

namespace base {

using std::ostringstream;
using std::ostream;

// A LogMessage is the object version of one line in the log file. We
// build the message with the methods here then have it push itself
// into the log.
//

class LogMessage {
public:
  // message levels ordered by severity ascending
  enum Severity { NORMAL=0, WARNING, ERROR, FATAL, MAX_SEVERITY };

  LogMessage(const char* file_name, const int file_line, Severity serverity);
  ~LogMessage();

  // Dumps the contents of 'msg_stream_' into the LogWriter singleton.
  void flush();

  // Returns an output stream where message contents could be piped
  // to.
  ostream& stream();

private:
  ostringstream msg_stream_;
  Severity severity_;
};

} // namespace base

#endif //  MCP_BASE_LOG_MESSAGE_HEADER
