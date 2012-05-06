#include "ticks_clock.hpp"

namespace base {

static double ticks_per_second = 0;

void moduleInit() {
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 500000000; // 500 ms
  TicksClock::Ticks before = TicksClock::getTicks();
  nanosleep(&ts, NULL);
  TicksClock::Ticks after = TicksClock::getTicks();
  ticks_per_second = (after - before) * 2;
}

pthread_once_t TicksClock::once_control = PTHREAD_ONCE_INIT;
long ticks_per_second_ = 0;

double TicksClock::ticksPerSecond() {
  pthread_once(&once_control, moduleInit);
  return ticks_per_second;
}

}  // namespace base
