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

TEST(ChangeHelpers, LeftOnlyOnTrueToFalse) {
  bool last = false;
  EXPECT_FALSE(left(last, false));  // a first false is not a turn off
  EXPECT_FALSE(left(last, true));
  EXPECT_TRUE(last);
  EXPECT_TRUE(left(last, false));
  EXPECT_FALSE(last);
  EXPECT_FALSE(left(last, false));
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

TEST(ChangeHelpers, ChangedComparesTextAndKeepsThePointer) {
  static constexpr const char *ECO = "eco";
  const char eco_copy[] = "eco";
  const char *last = nullptr;
  EXPECT_FALSE(changed(last, StringRef()));  // no text and an empty one are the same
  EXPECT_TRUE(changed(last, StringRef(ECO)));
  EXPECT_EQ(last, ECO);
  EXPECT_FALSE(changed(last, StringRef(eco_copy)));
  EXPECT_EQ(last, eco_copy);
  EXPECT_TRUE(changed(last, StringRef("boost")));
}

}  // namespace esphome::core::testing
