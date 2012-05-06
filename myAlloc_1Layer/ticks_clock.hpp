#ifndef DISTRIB_BASE_TICKS_CLOCK_HEADER
#define DISTRIB_BASE_TICKS_CLOCK_HEADER

#include <inttypes.h>
#include <time.h>

#include "logging.hpp"

namespace base {

// The TicksClock is a very low overhead to measure elapsed time at
// the nanosecond granularity. It uses the CPU's internal timestamp
// counter.
// 
// Usage: 
//    // Measure how long a given computation takes
//    Ticks before = TicksClock::getTicks();   
//    .... do computation ...
//    Ticks duration = TicksClock::getTicks() - before;
//
//    double in_seconds = duration / TicksClock::ticksPerSecond();
//

class TicksClock {
public:
  typedef uint64_t Ticks;

  static double ticksPerSecond();

  static Ticks getTicks() {
#if defined(__i386__) || defined(__x86_64__)
    uint32_t hi;
    uint32_t lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
#else
    LOG(LogMessage::ERROR) << "no support yet for ticks clock";
    return 0;
#endif
  }

private:
  static pthread_once_t once_control;
  
  TicksClock() { }
  ~TicksClock() { } 

  // Non-copyable, non-assignable
  TicksClock(TicksClock&);
  TicksClock& operator=(TicksClock&);
  
};

}  // namespace base

#endif // DISTRIB_BASE_TICKS_CLOCK_HEADER

