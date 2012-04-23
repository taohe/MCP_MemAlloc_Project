#ifndef MCP_BASE_LOG_WRITER_HEADER
#define MCP_BASE_LOG_WRITER_HEADER

#include <pthread.h>
#include <stdio.h>
#include <string>

#include "lock.hpp"

namespace base {

using std::ofstream;
using std::string;

class LogWriter
{
public:
  ~LogWriter();

  static LogWriter* instance();
  void write(const string& msg);
  
private:
  LogWriter();
  static void init();

  // REQUIRES: m_ is locked
  bool createFile();

  static pthread_once_t init_control_;
  static LogWriter*     instance_;      // not owned here
  Mutex                 m_;             // protects below
  FILE*                 log_file_;
};

}  // namespace base

#endif // MCP_BASE_LOG_WRITER_HEADER
