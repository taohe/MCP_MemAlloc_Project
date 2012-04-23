#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <fcntl.h>  // O_* constants
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/stat.h> // mode constants
#include <unistd.h>

#include <tr1/unordered_set>

#include "thread_registry.hpp"
#include "signal_handler.hpp"

namespace base {

using std::tr1::unordered_set;

// A flag to avoid running the handler recursively.
volatile static sig_atomic_t in_progress = 0;

// We use a semaphore to synchronize between the signal handler that
// originally received a SIGSEGV and any threads that may be running
// at a time.
static sem_t* sem;
static const char* sem_name = "signal_handler";

// We'll need static space inside the handler to hold the stack frames
// and strings we'd like to output.
static const int MAX_NUM_FRAMES = 16;
static const int MAX_ENTRY_SIZE = 256;
static void* frames[MAX_NUM_FRAMES];
static char entry[MAX_ENTRY_SIZE];

static void rawWrite(int fd, const char* msg, int len) {
  int bytes = 0;
  do {
    bytes = write(fd, msg, len);
  } while ((bytes <= 0) && (errno == EINTR));
}

void printStackTrace() {
  // Do not change the 'start backtrace' string
  // below. Signal_handler_test.cpp counts them for correctness.
  rawWrite(STDERR_FILENO, "==== start backtrace ====\n", 27);

  // Printstacktrace (1-frame) is called from fatalSingalHandler or
  // from dumpStackHandler (1-frame). We do not dump any of these
  // frames' information.
  int num_frames = backtrace(frames, MAX_NUM_FRAMES);
  if (num_frames > 1) {
    backtrace_symbols_fd(&frames[2], num_frames-2, STDERR_FILENO);
  } else {
    rawWrite(STDERR_FILENO, "Could not get stack trace\n", 27);
  }

  rawWrite(STDERR_FILENO, "==== end backtrace ====\n", 25);
}

void fatalSignalHandler(int sig, siginfo_t* siginfo, void* ucontext) {
  if (in_progress) {
    raise(sig);
  }
  in_progress = 1;

  int size =  snprintf(entry, MAX_ENTRY_SIZE, "Got signal %d\n", sig);
  rawWrite(STDERR_FILENO, entry, size);

  printStackTrace();

  // Force a run of 'dumpStackHandler' all other threads but the one
  // here. pthread_* calls are not async safe but at least we've printed
  // the stack of the offending code already.
  pthread_t my_tid = pthread_self();
  unordered_set<pthread_t> tids;
  ThreadRegistry::instance()->getAllThreadIDs(&tids);
  for (unordered_set<pthread_t>::iterator it = tids.begin();
       it != tids.end();
       ++it) {
    if (*it != my_tid) {
      pthread_kill(*it, SIGUSR1);
      sem_wait(sem);
    }
  }

  sem_close(sem);
  sem_unlink(sem_name);

  signal(sig, SIG_DFL);
  raise(sig);
}

void dumpStackHandler(int sig, siginfo_t* siginfo, void* ucontext) {
  printStackTrace();
  sem_post(sem);
}

void installSignalHandler() {
  sem_unlink(sem_name);

  mode_t mode = S_IRUSR | S_IWUSR;
  int flag = O_EXCL | O_CREAT;
  sem = sem_open(sem_name, flag, mode, 0);
  if (sem == SEM_FAILED) {
    perror("Can't initializae thread synchronization semaphore");
    exit(1);
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = fatalSignalHandler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    perror("Can't install SIGSEGV signal handler");
    exit(1);
  }

  sa.sa_sigaction = dumpStackHandler;
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("Can't install SIGSEGV signal handler");
    exit(1);
  }
}

} // namespace base
