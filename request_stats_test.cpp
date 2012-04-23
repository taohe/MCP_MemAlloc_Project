#include "ticks_clock.hpp"
#include "test_unit.hpp"

#include "request_stats.hpp"

namespace {

uint64_t kTicksPerSecond = base::TicksClock::ticksPerSecond();

TEST(Sequential, Intial) {
  base::RequestStats stats(1);

  // one request spawing from ticks 0..10
  stats.finishedRequest(0, 10);

  // check the request is there one tick later
  uint32_t req_rate = 0;
  stats.getStats(11, &req_rate);
  EXPECT_EQ(req_rate, 1);
}

TEST(Sequential, Stale) {
  base::RequestStats stats(1);

  // one request spawing from ticks 0..10
  stats.finishedRequest(0, 10);

  // check the request is there after one second
  uint32_t req_rate = 0;
  stats.getStats(kTicksPerSecond+1, &req_rate);
  EXPECT_EQ(req_rate, 0);
}

TEST(Sequential, SecondSlot) {
  base::RequestStats stats(1);

  // one request in the second slot
  uint64_t now = stats.ticksPerSlot()+1;
  stats.finishedRequest(0, now);

  // check the request is there when in second slot time
  uint32_t req_rate = 0;
  stats.getStats(now+1, &req_rate);
  EXPECT_EQ(req_rate, 1);

  // and later, when we rolled to the next slot already
  uint64_t later = stats.ticksPerSlot()*2+1;
  stats.getStats(later, &req_rate);
  EXPECT_EQ(req_rate, 1);
}

TEST(Sequential, RollOver) {
  base::RequestStats stats(1);

  // two request in the first slot
  uint64_t now = 20;
  stats.finishedRequest(0, now-2);
  stats.finishedRequest(0, now-1);
  uint32_t req_rate = 0;
  stats.getStats(now, &req_rate);
  EXPECT_EQ(req_rate, 2);

  // more than one second later the two requests above
  // should have expired
  now = now + kTicksPerSecond;
  stats.finishedRequest(0, now);
  stats.getStats(now, &req_rate);
  EXPECT_EQ(req_rate, 1);
}

}  // unamed namespace

int main(int argc, char *argv[]) {
  return RUN_TESTS(argc, argv);
}
