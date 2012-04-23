#ifndef MCP_BASE_CHILD_PROCESS_HEADER
#define MCP_BASE_CHILD_PROCESS_HEADER

#include <sys/types.h>
#include <sys/wait.h>

#include "callback.hpp"

namespace base {

// ChildProcess is a helper class to test abnormal termination
// scenarios. It can process the code to be tested on a child process,
// which can then terminate independently of the testing parent
// process.
//
// After terminatinon, the parent process can, for instance, examine
// the child's standard error and verify a give error was produces.
//
// Usage example: see signal_handler_test.cpp
//
class ChildProcess {
public:
  // Records that, when start()-ed, we'd like the child process to run
  // 'child_cb'. Does not fork the child process yet.
  explicit ChildProcess(Callback<void>* child_cb);
  ~ChildProcess();

  // Forks a chils process to run 'child_cb_' and creates a pipe between
  // child's STDERR and the parent process.
  bool start();

  // Blocks until the child process initiated by 'start()' finishes
  // and returns its exit code. The code can be tested with the waitpid()
  // associated macros (e.g. WIFSIGNALED).
  int wait();

  // Returns the reading side (parent's) of the STDERR pipe
  // established between child and parent processes.
  int getStderrFD();

private:
  pid_t           child_pid_;
  Callback<void>* child_cb_;  // not owned here
  int             pfd_[2];    // stderr pipe
};


} // namespace base

#endif // MCP_BASE_CHILD_PROCESS_HEADER
