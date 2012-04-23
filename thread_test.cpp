#include "test_unit.hpp"
#include "thread.hpp"

namespace {

TEST(Group, Case) {
  EXPECT_TRUE(true);
}

} // unnammed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
