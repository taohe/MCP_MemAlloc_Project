#include <pthread.h>
#include <unistd.h>  // pause, WIFSIGNALED

#include <ifdstream.hpp>
#include <string>

#include "callback.hpp"
#include "child_process.hpp"
#include "signal_handler.hpp"
#include "test_unit.hpp"
#include "thread.hpp"

namespace {

using std::fdistream;
using std::string;

using base::Callback;
using base::ChildProcess;
using base::makeCallableOnce;
using base::makeCallableMany;
using base::makeThread;

class LoopOrSegv {
public:
  LoopOrSegv() : cb_(makeCallableMany(&LoopOrSegv::foo, this)) {}
  ~LoopOrSegv() { delete cb_; }

  void run() {
    pthread_t tid1 = makeThread(cb_);
    pthread_t tid2 = makeThread(cb_);

    int* p = NULL;
    *p = 0;

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
  }

  void foo() { bar(); }
  void bar() { loop(); }
  void loop() { while (true) pause(); }

private:
  Callback<void>* cb_;
};

TEST(Crash, ReportThreads){
  LoopOrSegv* segv = new LoopOrSegv;
  Callback<void>* cb = makeCallableMany(&LoopOrSegv::run, segv);

  // Fork a child process that will run three threads. Two of them in
  // 'loop' and one in the segv producing stack.
  ChildProcess child(cb);
  EXPECT_TRUE(child.start());
  int status = child.wait();
  EXPECT_TRUE(WIFSIGNALED(status));
  //EXPECT_EQ(WTERMSIG(status), 11);

  // Parse the child's stderr log and check that the three stacks were
  // dumped.
  int thread_count = 0;
  string line;
  fdistream is(child.getStderrFD());
  while (!is.eof()) {
    getline(is, line);
    if (line.find("start backtrace") != string::npos) {
      ++thread_count;
    }
  }
  EXPECT_EQ(thread_count, 3);

  delete cb;
  delete segv;
}

} // unnamed namespace

int main(int argc, char* argv[]) {
  base::installSignalHandler();
  return RUN_TESTS(argc, argv);
}
