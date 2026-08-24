#include <gtest/gtest.h>

#include "esphome/components/atm90e32/atm90e32.h"

namespace esphome::atm90e32::testing {

TEST(ATM90E32OffsetRegisterVerification, AcceptsExactSignedReadback) {
  EXPECT_TRUE(offset_register_value_matches(0x007B, 123));
  EXPECT_TRUE(offset_register_value_matches(0xFF85, -123));
}

TEST(ATM90E32OffsetRegisterVerification, RejectsMismatchedReadback) {
  EXPECT_FALSE(offset_register_value_matches(0x007C, 123));
  EXPECT_FALSE(offset_register_value_matches(0xFF84, -123));
}

TEST(ATM90E32OffsetPersistence, PreservesStoredLayout) {
  EXPECT_EQ(sizeof(OffsetCalibration[3]), 12U);
}

TEST(ATM90E32OffsetPersistence, RollsBackStoredValuesOrZeroSentinel) {
  const OffsetCalibration previous[3]{{1, -1}, {2, -2}, {3, -3}};
  OffsetCalibration rollback[3]{};

  prepare_offset_rollback(previous, true, rollback);
  for (uint8_t phase = 0; phase < 3; phase++) {
    EXPECT_EQ(rollback[phase].first_offset, previous[phase].first_offset);
    EXPECT_EQ(rollback[phase].second_offset, previous[phase].second_offset);
  }

  prepare_offset_rollback(previous, false, rollback);
  for (const auto &phase : rollback) {
    EXPECT_EQ(phase.first_offset, 0);
    EXPECT_EQ(phase.second_offset, 0);
  }
}

}  // namespace esphome::atm90e32::testing
