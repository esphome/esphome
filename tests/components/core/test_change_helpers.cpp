#include <gtest/gtest.h>

#include <cmath>

#include "esphome/core/automation.h"

namespace esphome::core::testing {

TEST(ChangeHelpers, EnteredOnlyOnFalseToTrue) {
  bool last = false;
  EXPECT_FALSE(internal::change_trigger::entered(last, false));
  EXPECT_TRUE(internal::change_trigger::entered(last, true));
  EXPECT_TRUE(last);
  EXPECT_FALSE(internal::change_trigger::entered(last, true));
  EXPECT_FALSE(internal::change_trigger::entered(last, false));
  EXPECT_FALSE(last);
  EXPECT_TRUE(internal::change_trigger::entered(last, true));
}

TEST(ChangeHelpers, LeftOnlyOnTrueToFalse) {
  bool last = false;
  EXPECT_FALSE(internal::change_trigger::left(last, false));  // a first false is not a turn off
  EXPECT_FALSE(internal::change_trigger::left(last, true));
  EXPECT_TRUE(last);
  EXPECT_TRUE(internal::change_trigger::left(last, false));
  EXPECT_FALSE(last);
  EXPECT_FALSE(internal::change_trigger::left(last, false));
}

TEST(ChangeHelpers, ChangedOnEveryNewValue) {
  int last = 0;
  EXPECT_FALSE(internal::change_trigger::changed(last, 0));
  EXPECT_TRUE(internal::change_trigger::changed(last, 3));
  EXPECT_EQ(last, 3);
  EXPECT_FALSE(internal::change_trigger::changed(last, 3));
  EXPECT_TRUE(internal::change_trigger::changed(last, -1));
  EXPECT_EQ(last, -1);
}

TEST(ChangeHelpers, ChangedComparesByValue) {
  float last = 0.0f;
  EXPECT_TRUE(internal::change_trigger::changed(last, 0.5f));
  EXPECT_FALSE(internal::change_trigger::changed(last, 0.5f));
  enum class Direction : uint8_t { DIRECTION_FORWARD, DIRECTION_REVERSE };
  Direction direction = Direction::DIRECTION_FORWARD;
  EXPECT_TRUE(internal::change_trigger::changed(direction, Direction::DIRECTION_REVERSE));
  EXPECT_FALSE(internal::change_trigger::changed(direction, Direction::DIRECTION_REVERSE));
}

TEST(ChangeHelpers, ChangedComparesTextAndKeepsThePointer) {
  static constexpr const char *ECO = "eco";
  const char eco_copy[] = "eco";
  const char *last = nullptr;
  EXPECT_FALSE(internal::change_trigger::changed(last, ""));  // no text and an empty one are the same
  EXPECT_FALSE(internal::change_trigger::changed(last, static_cast<const char *>(nullptr)));
  EXPECT_TRUE(internal::change_trigger::changed(last, ECO));
  EXPECT_EQ(last, ECO);
  EXPECT_FALSE(internal::change_trigger::changed(last, eco_copy));
  EXPECT_EQ(last, eco_copy);
  EXPECT_TRUE(internal::change_trigger::changed(last, "boost"));
}

TEST(ChangeHelpers, ChangedComparesWholeStrings) {
  const char economy[] = "economy";
  const char economy_copy[] = "economy";
  const char *last = "eco";
  // A prefix is a different text, in either direction
  EXPECT_TRUE(internal::change_trigger::changed(last, economy));
  EXPECT_FALSE(internal::change_trigger::changed(last, economy_copy));
  EXPECT_TRUE(internal::change_trigger::changed(last, "eco"));
}

// A watched value converts to the copy's type when that does not narrow it; a narrowing one does not compile
TEST(ChangeHelpers, ChangedWidensTheWatchedValueToTheCopysType) {
  int last = 0;
  const uint8_t watched = 3;
  EXPECT_TRUE(internal::change_trigger::changed(last, watched));
  EXPECT_EQ(last, 3);
  EXPECT_FALSE(internal::change_trigger::changed(last, watched));
}
template<typename T, typename U>
concept TracksChange = requires(T &last, U now) { internal::change_trigger::changed(last, now); };
static_assert(TracksChange<int, uint8_t>);
static_assert(!TracksChange<uint8_t, int>);

TEST(ChangeHelpers, ChangedTreatsNanAsUnchanged) {
  float last = 0.0f;
  EXPECT_TRUE(internal::change_trigger::changed(last, NAN));
  EXPECT_FALSE(internal::change_trigger::changed(last, NAN));
  EXPECT_TRUE(internal::change_trigger::changed(last, 1.0f));
  EXPECT_FALSE(internal::change_trigger::changed(last, 1.0f));
}

// Text keeps the text comparison for a mutable buffer or nullptr too, not the pointer comparison
TEST(ChangeHelpers, ChangedComparesTextForCharBuffersAndNullptr) {
  char first[] = "eco";
  char second[] = "eco";
  const char *last = nullptr;
  EXPECT_TRUE(internal::change_trigger::changed(last, first));
  EXPECT_FALSE(internal::change_trigger::changed(last, second));
  const char *empty = "";
  EXPECT_FALSE(internal::change_trigger::changed(empty, nullptr));
}

}  // namespace esphome::core::testing
