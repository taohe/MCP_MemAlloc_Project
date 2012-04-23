#include <algorithm>
#include <sstream>

#include "request_stats.hpp"

namespace base {

using std::cout;
using std::endl;
using std::min;
using std::ostringstream;

const uint32_t RequestStats::kNumSlots;
const uint64_t RequestStats::kTicksPerSlot =
  TicksClock::ticksPerSecond() / kNumSlots;

RequestStats::RequestStats(int num_threads) : num_threads_(num_threads) {
  counts_ = new Counts[num_threads_];
  for (int i=0; i<num_threads_; ++i) {
    counts_[i].base_pos = 0;
    counts_[i].base_tick = 0;
    for (size_t j=0; j<kNumSlots; ++j) {
      counts_[i].val[j] = 0;
    }
  }
}

RequestStats::~RequestStats() {
  delete[] counts_;
}

void RequestStats::finishedRequest(int thread_num, TicksClock::Ticks now) {
  if (thread_num >= num_threads_) {
    LOG(LogMessage::FATAL) << "wrong thread num";
  }
  Counts& counts = counts_[thread_num];

  // Has at least one slot expired between the last and this request?
  if (now > counts.base_tick + kTicksPerSlot) {

    uint32_t new_pos = posForTick(now);

    // If the last request came in more than a second ago, just zero
    // all the slots. Otherwise, zero the slots between the last
    // request and the one we're handling now.
    if (counts.base_tick + TicksClock::ticksPerSecond() < now) {
      for (size_t i=0; i<kNumSlots; ++i) {
        counts.val[i] = 0;
      }
    } else {
      uint32_t curr_pos = counts.base_pos;
      do {
        curr_pos = incPos(curr_pos);
        counts.val[curr_pos] = 0;
      } while (curr_pos != new_pos);
    }

    // Adjust the position and tick mark of the last slot now.
    counts.base_pos = new_pos;
    counts.base_tick = roundTick(now);
  }

  ++counts.val[counts.base_pos];
}

void RequestStats::getStats(TicksClock::Ticks now,
                            uint32_t* reqsLastSec ) const {
  uint32_t reqs_acc = 0;

  // We want to read only the slots that have not expired. We assume
  // that all threads started recording a request at tick 0.
  for (int i=0; i<num_threads_; ++i) {

    const Counts& counts = counts_[i];

    // If the last request on this thread come in more than a second
    // ago, don't bother checking the thread. Otherwise, accumulate
    // the slots that are within the last second.
    if (now - counts.base_tick > TicksClock::ticksPerSecond()) {
      continue;
    } else {
      // posForTick(now - 1sec) == posForTick(now)
      uint32_t curr_pos = posForTick(now);
      do {
        curr_pos = incPos(curr_pos);
        reqs_acc += counts.val[curr_pos];
      } while (curr_pos != counts.base_pos);
    }
  }

  *reqsLastSec = reqs_acc;
}

} // namespace base
