#include <gtest/gtest.h>

#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/logger/logger.h"

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

class FailedGainRollbackDelegate final : public spi::SPIDelegate {
 public:
  uint8_t transfer(uint8_t data) override { return 0; }
};

class ATM90E32GainCalibrationTest : public ::testing::Test {
 protected:
  void check_failed_rollback_() {
    for (const bool mismatch : {false, true}) {
      for (uint8_t offsets = 0; offsets < 4; offsets++) {
        SCOPED_TRACE(::testing::Message() << "mismatch=" << mismatch << ", offsets=" << +offsets);
        FailedGainRollbackDelegate delegate;
        ATM90E32Component component;
        component.delegate_ = &delegate;
        component.instance_id_ = "test";
        component.enable_gain_calibration_ = true;
        component.enable_offset_calibration_ = true;
        component.restored_gain_calibration_ = true;
        component.has_stored_gain_calibration_ = true;
        component.using_saved_calibrations_ = true;
        component.restored_offset_calibration_ = (offsets & 1) != 0;
        component.restored_power_offset_calibration_ = (offsets & 2) != 0;
        for (bool &phase : component.gain_calibration_mismatch_)
          phase = mismatch;
        const GainCalibration previous[3]{{7305, 27518}, {7305, 27518}, {7305, 27518}};

        component.finish_gain_calibration_(previous, true, true);

        EXPECT_FALSE(component.restored_gain_calibration_);
        EXPECT_TRUE(component.has_stored_gain_calibration_);
        EXPECT_EQ(component.using_saved_calibrations_, offsets != 0);
        EXPECT_EQ(component.restored_offset_calibration_, (offsets & 1) != 0);
        EXPECT_EQ(component.restored_power_offset_calibration_, (offsets & 2) != 0);
        for (const bool phase : component.gain_calibration_mismatch_)
          EXPECT_FALSE(phase);

        const auto previous_baud_rate = logger::global_logger->get_baud_rate();
        ::testing::internal::CaptureStdout();
        logger::global_logger->set_baud_rate(115200);
        component.dump_config();
        logger::global_logger->set_baud_rate(previous_baud_rate);
        const auto output = ::testing::internal::GetCapturedStdout();
        EXPECT_EQ(output.find("Gain calibration loaded and verified successfully."), std::string::npos);
        EXPECT_EQ(output.find("Gain mismatch: using flash values"), std::string::npos);
        EXPECT_EQ(output.find("Restoring saved gain calibrations to registers"), std::string::npos);
      }
    }
  }
};

TEST_F(ATM90E32GainCalibrationTest, FailedRollbackDoesNotReportVerifiedGainsOnReconnect) {
  this->check_failed_rollback_();
}

}  // namespace esphome::atm90e32::testing
