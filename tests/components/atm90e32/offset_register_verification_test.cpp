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

TEST(ATM90E32OffsetRestoreState, ReportsVerifiedStoredValuesAsRestored) {
  const auto state = resolve_calibration_restore_state(true, true, false);

  EXPECT_TRUE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsVerifiedConfigFallbackAsNotRestored) {
  const auto state = resolve_calibration_restore_state(true, false, true);

  EXPECT_FALSE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsFailedConfigFallbackAsUnverified) {
  const auto state = resolve_calibration_restore_state(true, false, false);

  EXPECT_FALSE(state.restored);
  EXPECT_FALSE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsConfigWithoutStoredValuesAsNotRestored) {
  const auto state = resolve_calibration_restore_state(false, true, false);

  EXPECT_FALSE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetPersistence, RollsBackStoredValuesOrZeroSentinel) {
  const OffsetCalibration previous[3]{{1, -1}, {2, -2}, {3, -3}};
  OffsetCalibration rollback[3]{};

  prepare_calibration_rollback(previous, true, OffsetCalibration{}, rollback);
  for (uint8_t phase = 0; phase < 3; phase++) {
    EXPECT_EQ(rollback[phase].first_offset, previous[phase].first_offset);
    EXPECT_EQ(rollback[phase].second_offset, previous[phase].second_offset);
  }

  prepare_calibration_rollback(previous, false, OffsetCalibration{}, rollback);
  for (const auto &phase : rollback) {
    EXPECT_EQ(phase.first_offset, 0);
    EXPECT_EQ(phase.second_offset, 0);
  }
}

TEST(ATM90E32GainPersistence, RollsBackStoredValuesOrZeroSentinel) {
  const GainCalibration previous[3]{{101, 201}, {102, 202}, {103, 203}};
  GainCalibration rollback[3]{};

  prepare_calibration_rollback(previous, true, GainCalibration{0, 0}, rollback);
  for (uint8_t phase = 0; phase < 3; phase++) {
    EXPECT_EQ(rollback[phase].voltage_gain, previous[phase].voltage_gain);
    EXPECT_EQ(rollback[phase].current_gain, previous[phase].current_gain);
  }

  prepare_calibration_rollback(previous, false, GainCalibration{0, 0}, rollback);
  for (const auto &phase : rollback) {
    EXPECT_EQ(phase.voltage_gain, 0);
    EXPECT_EQ(phase.current_gain, 0);
  }
}

}  // namespace esphome::atm90e32::testing
