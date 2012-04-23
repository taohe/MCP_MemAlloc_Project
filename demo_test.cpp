#include <inttypes.h>
#include <limits>
#include "test_unit.hpp"

namespace {

int64_t add(int32_t a, int32_t b) {
  int64_t res = a;
  res += b;
  return res;
}

// AL: comment 
TEST(Simple, WithinBounds) {
  EXPECT_EQ(2, add(1, 1));
}

TEST(Simple, Zero) {
  EXPECT_EQ(0, add(0, 0));
}

TEST(Negative, OneArgument) {
  EXPECT_EQ(1, add(2, -1));
}

TEST(Negative, ArgumentAndResult) {
  EXPECT_EQ(-2, add(1, -3));
}

TEST(Overflow, MaxArgument) {
  int32_t max = std::numeric_limits<int32_t>::max();
  int64_t sum = add(max, 1);
  EXPECT_GT(sum, max);
}

} // unnamed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
