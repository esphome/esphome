#include <gtest/gtest.h>
#include <cmath>

#include "esphome/core/automation.h"

namespace esphome::testing {

TEST(ValueRange, BothBoundsAreInclusive) {
  const ValueRange range{10.0f, 20.0f};
  EXPECT_FALSE(range.contains(9.9f, true));
  EXPECT_TRUE(range.contains(10.0f, false));
  EXPECT_TRUE(range.contains(20.0f, false));
  EXPECT_FALSE(range.contains(20.1f, true));
}

TEST(ValueRange, NanBoundIsNotChecked) {
  EXPECT_TRUE((ValueRange{10.0f, NAN}.contains(1000.0f, false)));
  EXPECT_FALSE((ValueRange{10.0f, NAN}.contains(9.0f, true)));
  EXPECT_TRUE((ValueRange{NAN, 10.0f}.contains(-1000.0f, false)));
  EXPECT_FALSE((ValueRange{NAN, 10.0f}.contains(11.0f, true)));
}

TEST(ValueRange, BothNanKeepsThePreviousResult) {
  const ValueRange range{NAN, NAN};
  EXPECT_TRUE(range.contains(5.0f, true));
  EXPECT_FALSE(range.contains(5.0f, false));
}

TEST(ValueRange, WidthIsMaxMinusMin) {
  EXPECT_FLOAT_EQ((ValueRange{10.0f, 30.0f}.width()), 20.0f);
  EXPECT_TRUE(std::isnan(ValueRange{10.0f, NAN}.width()));
}

}  // namespace esphome::testing
