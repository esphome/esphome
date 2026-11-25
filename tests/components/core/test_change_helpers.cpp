#include <gtest/gtest.h>

#include "esphome/core/automation.h"

namespace esphome::core::testing {

TEST(ChangeHelpers, EnteredOnlyOnFalseToTrue) {
  bool last = false;
  EXPECT_FALSE(entered(last, false));
  EXPECT_TRUE(entered(last, true));
  EXPECT_TRUE(last);
  EXPECT_FALSE(entered(last, true));
  EXPECT_FALSE(entered(last, false));
  EXPECT_FALSE(last);
  EXPECT_TRUE(entered(last, true));
}

TEST(ChangeHelpers, EachCopyIsItsOwn) {
  bool first = false;
  bool second = false;
  EXPECT_TRUE(entered(first, true));
  // A copy that missed a report still sees the value it last saw
  EXPECT_FALSE(entered(second, false));
  EXPECT_FALSE(entered(first, true));
  EXPECT_TRUE(entered(second, true));
}

TEST(ChangeHelpers, ChangedOnEveryNewValue) {
  int last = 0;
  EXPECT_FALSE(changed(last, 0));
  EXPECT_TRUE(changed(last, 3));
  EXPECT_EQ(last, 3);
  EXPECT_FALSE(changed(last, 3));
  EXPECT_TRUE(changed(last, -1));
  EXPECT_EQ(last, -1);
}

TEST(ChangeHelpers, ChangedComparesByValue) {
  float last = 0.0f;
  EXPECT_TRUE(changed(last, 0.5f));
  EXPECT_FALSE(changed(last, 0.5f));
  enum class Direction : uint8_t { FORWARD, REVERSE };
  Direction direction = Direction::FORWARD;
  EXPECT_TRUE(changed(direction, Direction::REVERSE));
  EXPECT_FALSE(changed(direction, Direction::REVERSE));
}

}  // namespace esphome::core::testing
