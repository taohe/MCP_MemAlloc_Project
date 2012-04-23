#ifndef MCP_REQUEST_STATS_HEADER
#define MCP_REQUEST_STATS_HEADER

#include <inttypes.h>
#include <string>

#include "ticks_clock.hpp"

namespace base {

using base::TicksClock;
using std::string;

// This class keeps enough state to compute the number of request
// served per second instantaneously. That is, when the stats is
// requested, the second finishing _roughly_ at the request time is
// used.
//
// Thread Safety:
//
//   We assume that finishedRequest(i,...) is only called by the i-th
//   thread, according to ThreadPoolFast::ME(). Calls to
//   finishRequest(j,...)  can be done concurrently with the former.
//
//   getStats() will be called at any time by some unknow thread. For
//   the lab4's purposes, we'll assume it can read the internal state
//   without synchronization (we'll reason about what could happen to
//   accuracy of the results).

class RequestStats {
public:
  explicit RequestStats(int num_threads);
  ~RequestStats();

  // Records that one request completed 'now'.  The thread calling
  // this should use it's identifier (returned by
  // ThreadPoolFast::ME())
  void finishedRequest(int thread_num, TicksClock::Ticks now);

  // Writes the current req/s stats for the second finishing 'now' in
  // 'reqsLastSec'.
  void getStats(TicksClock::Ticks now, uint32_t* reqsLastSec) const;

  // Accessors (for testing)

  uint64_t ticksPerSlot() const { return 0; }

private:
  const int num_threads_;

  // Number of slots we divide our second in. We use slotting in a
  // circular structure to allow for a rolling window of 1 second.
  static const uint32_t kNumSlots = 10;

  // Given that we'll split one second in 'kNumSlots', how many
  // ticks do we have in one slot.
  static const uint64_t kTicksPerSlot;

  // We'll pad each instance of 'Count' below assuming the 64 bytes
  // cache line.
  static const int kLineSizeInBytes = 64;

  // A circular vector of accumulators that record how many requests
  // were received in a give slot of time, during a 1 second rolling
  // window of time.
  struct Counts {
    uint32_t base_pos;              // "now" slot in the val array (circular)
    uint64_t base_tick;             // ticks when "now" started
    uint32_t val[kNumSlots];        // partial counts of requests

    uint8_t pad[kLineSizeInBytes];  // pad to avoid false sharing of lines
  };
  Counts* counts_;                  // owned here

  // Helpers

  // Returns the next position to 'p' in the circular vector.
  uint32_t incPos(uint32_t p) const { return (p + 1) % kNumSlots; }

  // Returns the position a request arrived at tick 't' should occupy.
  uint32_t posForTick(uint64_t t) const { return t/kTicksPerSlot % kNumSlots; }

  // Returns the tick previous to tick 't'  that would fall right in the
  // start of a slot.
  uint64_t roundTick(uint64_t t) const { return t - (t % kTicksPerSlot); }
};

}  // namespace base

#endif // MCP_REQUEST_STATS_HEADER
