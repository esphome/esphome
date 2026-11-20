#include <gtest/gtest.h>

#include "esphome/components/copy/number/copy_number.h"

namespace esphome::copy::testing {

static void ignore_value(number::Number * /*entity*/, float /*value*/) {}

TEST(CopyNumber, FollowsTheSourceRangeAndWritesItsOwn) {
  number::NumberRange storage{};
  number::ApplyNumber<ignore_value> source;
  source.traits.set_range_storage(&storage);
  CopyNumber copy;
  copy.set_source_number(&source);
  copy.setup();

  // Bounds the source gets after setup (from Home Assistant) show up in the copy
  source.traits.set_min_value(1.0f);
  source.traits.set_max_value(9.0f);
  source.traits.set_step(0.5f);
  EXPECT_EQ(copy.traits.get_min_value(), 1.0f);
  EXPECT_EQ(copy.traits.get_max_value(), 9.0f);
  EXPECT_EQ(copy.traits.get_step(), 0.5f);

  // Changing the copy's bounds leaves the source alone
  copy.traits.set_max_value(5.0f);
  EXPECT_EQ(copy.traits.get_max_value(), 5.0f);
  EXPECT_EQ(source.traits.get_max_value(), 9.0f);
}

}  // namespace esphome::copy::testing
