#include "list_set.hpp"
#include "test_unit.hpp"

namespace {

using base::ListBasedSet;

TEST(Simple, Insertion) {
  ListBasedSet s;
  EXPECT_TRUE(s.insert(99));
}

} // unnamed namespace

int main(int argc, char* argv[]) {
  return RUN_TESTS(argc, argv);
}
