#include "callback.hpp"
#include "test_unit.hpp"
#include "test_util.hpp"

namespace {

using base::Callback;
using base::makeCallableOnce;
using base::makeCallableMany;
using test::Counter;

TEST(Once, Simple) {
  Counter c;
  Callback<void>* cb = makeCallableOnce(&Counter::inc, &c);
  EXPECT_TRUE(cb->once());
  (*cb)();
  EXPECT_EQ(c.count(), 1);
}

TEST(Once, Binding) {
  // early
  Counter c;
  Callback<void>* cb1 = makeCallableOnce(&Counter::incBy, &c, 2);
  EXPECT_TRUE(cb1->once());
  (*cb1)();
  EXPECT_EQ(c.count(), 2);

  // late
  c.reset();
  Callback<void, int>* cb2 = makeCallableOnce(&Counter::incBy, &c);
  EXPECT_TRUE(cb2->once());
  (*cb2)(3);
  EXPECT_EQ(c.count(), 3);
}

TEST(Once, Currying) {
  Counter c;
  Callback<void, int>* cb1 = makeCallableOnce(&Counter::incBy, &c);
  Callback<void>* cb2 =
    makeCallableOnce(&Callback<void,int>::operator(), cb1, 4);
  (*cb2)();
  EXPECT_EQ(c.count(), 4);
}

TEST(Once, ReturnType) {
  Counter c;
  c.set(7);
  Callback<bool, int, int>* cb1 = makeCallableOnce(&Counter::between, &c);
  EXPECT_TRUE((*cb1)(5, 10));

  Callback<bool, int>* cb2 = makeCallableOnce(&Counter::between, &c, 5);
  EXPECT_TRUE(cb2->once());
  EXPECT_TRUE((*cb2)(10));

  Callback<bool>* cb3 = makeCallableOnce(&Counter::between, &c, 5, 10);
  EXPECT_TRUE((*cb3)());
}

TEST(Many, Simple) {
  Counter c;
  Callback<void>* cb = makeCallableMany(&Counter::inc, &c);
  EXPECT_FALSE(cb->once());
  (*cb)();
  (*cb)();
  EXPECT_EQ(c.count(), 2);
  delete cb;
}

} // unnamed namespace

int main(int argc, char *argv[]) {
  return RUN_TESTS(argc, argv);
}
