#ifndef MCP_BASE_TIMER_HEADER
#define MCP_BASE_TIMER_HEADER

#include <sys/time.h>

namespace base {

// A microsecond precision timer based on the gettimeofday() call
// (which should be low overhead).
//
// Usage:
//
//   Timer t;
//   t.start();
//   ... event we want to clock
//   t.end();
//
//   std::cout << "elapsed time in seconds" << t.elapsed();
//

class Timer {
public:
  Timer();
  ~Timer();

  void start();
  void end();
  void reset();
  double elapsed() const;

private:
  struct timeval start_;
  struct timeval end_;
};

Timer::Timer() {
  reset();
}

Timer::~Timer() {
}

void Timer::reset() {
  start_.tv_sec = 0;
  start_.tv_usec = 0;
  end_.tv_sec = 0;
  end_.tv_usec = 0;
}

void Timer::start() {
  gettimeofday(&start_, NULL);
}

void Timer::end() {
  gettimeofday(&end_, NULL);
}

double Timer::elapsed() const {
  // assumes end_ >= start_

  double sec = 0;
  double usec = 0;
  if (end_.tv_usec < start_.tv_usec) {
    sec = end_.tv_sec - 1 - start_.tv_sec;
    usec = (end_.tv_usec + 1000000 - start_.tv_usec) / 1000000.0;
  } else {
    sec = end_.tv_sec - start_.tv_sec;
    usec = (end_.tv_usec - start_.tv_usec) / 1000000.0;
  }
  return sec+usec;
}

} // namespace base

#endif // MCP_BASE_TIMER_HEADER
