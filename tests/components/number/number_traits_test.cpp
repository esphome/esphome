#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/number/number.h"
#include "esphome/components/number/number_traits.h"

namespace esphome::number::testing {

static constexpr NumberRange TABLE[] = {{0.0f, 10.0f, 1.0f}};

TEST(NumberTraits, BoundsStartUnset) {
  NumberTraits traits;
  EXPECT_TRUE(std::isnan(traits.get_min_value()));
  EXPECT_TRUE(std::isnan(traits.get_max_value()));
  EXPECT_TRUE(std::isnan(traits.get_step()));
}

TEST(NumberTraits, RangeReadsTheTable) {
  NumberTraits traits;
  traits.set_range(TABLE);
  EXPECT_EQ(traits.get_min_value(), 0.0f);
  EXPECT_EQ(traits.get_max_value(), 10.0f);
  EXPECT_EQ(traits.get_step(), 1.0f);
}

TEST(NumberTraits, SetterCopiesTheTableAndLeavesItAlone) {
  NumberTraits traits;
  NumberTraits other;
  traits.set_range(TABLE);
  other.set_range(TABLE);
  traits.set_max_value(20.0f);
  traits.set_step(0.5f);
  EXPECT_EQ(traits.get_min_value(), 0.0f);
  EXPECT_EQ(traits.get_max_value(), 20.0f);
  EXPECT_EQ(traits.get_step(), 0.5f);
  // The shared table and the other number sharing it are unchanged
  EXPECT_EQ(TABLE[0].max_value, 10.0f);
  EXPECT_EQ(other.get_max_value(), 10.0f);
  EXPECT_EQ(other.get_step(), 1.0f);
}

TEST(NumberTraits, SettersWithoutATableStartFromUnset) {
  NumberTraits traits;
  traits.set_min_value(-5.0f);
  EXPECT_EQ(traits.get_min_value(), -5.0f);
  EXPECT_TRUE(std::isnan(traits.get_max_value()));
  traits.set_max_value(5.0f);
  traits.set_step(0.25f);
  EXPECT_EQ(traits.get_max_value(), 5.0f);
  EXPECT_EQ(traits.get_step(), 0.25f);
}

TEST(NumberTraits, RangeAfterASetterOverwritesTheCopy) {
  NumberTraits traits;
  traits.set_min_value(-5.0f);
  traits.set_range(TABLE);
  EXPECT_EQ(traits.get_min_value(), 0.0f);
  EXPECT_EQ(traits.get_max_value(), 10.0f);
  // Still a private copy, so the table stays untouched
  traits.set_step(2.0f);
  EXPECT_EQ(traits.get_step(), 2.0f);
  EXPECT_EQ(TABLE[0].step, 1.0f);
}

TEST(NumberTraits, StorageTakesLaterWrites) {
  NumberRange storage{};
  NumberTraits traits;
  traits.set_range_storage(&storage);
  EXPECT_EQ(traits.get_range(), &storage);
  EXPECT_EQ(traits.get_max_value(), 0.0f);
  traits.set_max_value(30.0f);
  EXPECT_EQ(storage.max_value, 30.0f);
  // A table given afterwards is copied into the storage, never written
  traits.set_range(TABLE);
  EXPECT_EQ(traits.get_range(), &storage);
  EXPECT_EQ(storage.max_value, 10.0f);
}

TEST(NumberTraits, SharedRangeFollowsTheSource) {
  NumberRange storage{};
  NumberTraits source;
  source.set_range_storage(&storage);
  NumberTraits copy;
  copy.set_range(source.get_range());
  // Bounds the source gets later (from Home Assistant) show up in the copy
  source.set_min_value(1.0f);
  source.set_max_value(2.0f);
  EXPECT_EQ(copy.get_min_value(), 1.0f);
  EXPECT_EQ(copy.get_max_value(), 2.0f);
}

TEST(NumberTraits, CopyWritesToItsOwnRange) {
  NumberTraits original;
  original.set_max_value(10.0f);
  NumberTraits copy = original;
  copy.set_max_value(20.0f);
  EXPECT_EQ(original.get_max_value(), 10.0f);
  EXPECT_EQ(copy.get_max_value(), 20.0f);

  NumberRange storage{};
  NumberTraits with_storage;
  with_storage.set_range_storage(&storage);
  NumberTraits assigned;
  assigned = with_storage;
  assigned.set_step(0.5f);
  EXPECT_EQ(storage.step, 0.0f);
  EXPECT_EQ(assigned.get_step(), 0.5f);
}

static void ignore_value(Number * /*entity*/, float /*value*/) {}

TEST(NumberTraits, CodegenTableAfterStorageIsNeverWritten) {
  NumberRange storage{};
  ApplyNumber<ignore_value> number;
  number.traits.set_range_storage(&storage);
  number.set_range(TABLE);
  number.traits.set_max_value(50.0f);
  EXPECT_EQ(number.traits.get_max_value(), 50.0f);
  EXPECT_EQ(TABLE[0].max_value, 10.0f);
}

TEST(NumberTraits, HoldsAPointerInsteadOfThreeFloats) { EXPECT_LE(sizeof(NumberTraits), 2 * sizeof(void *)); }

}  // namespace esphome::number::testing
