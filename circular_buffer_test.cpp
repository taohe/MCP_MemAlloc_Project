#include "circular_buffer.hpp"
#include "test_unit.hpp"

namespace {

using base::CircularBuffer;

TEST(Simple, ReadWrite) {
  CircularBuffer b(1);
  b.write(0);
  EXPECT_EQ(0, b.read());
}

} // unnamed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
