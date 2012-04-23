#include <cstdio>   // perror
#include <unistd.h> // pipe, fork, close, dup2

#include "child_process.hpp"

namespace base {

ChildProcess::ChildProcess(Callback<void>* child_cb)
  : child_pid_(-1), child_cb_(child_cb) {
}

ChildProcess::~ChildProcess() {
}

bool ChildProcess::start() {
  if (pipe(pfd_) == -1) {
    perror("Can't open pipes");
    return false;
  }

  child_pid_ = fork();
  switch(child_pid_) {
  case -1:
    perror("Error creating the child");
    return false;

  case 0:
    // Child will will report errors on the writing side the pipe.
    close(pfd_[0]);
    dup2(pfd_[1], STDERR_FILENO);
    close(pfd_[1]);

    (*child_cb_)();
    break;

  default:
    // Parent will be reading errors on the reading side of the pipe.
    close(pfd_[1]);
    return true;
  }
  return true;
}

int ChildProcess::wait() {
  int status;
  return  waitpid(child_pid_, &status, WUNTRACED);
}

int ChildProcess::getStderrFD() {
  return pfd_[0];
}

}  // namespace base
