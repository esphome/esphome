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

TEST(ATM90E32CalibrationPersistence, RequiresSaveAndSync) {
  EXPECT_TRUE(calibration_persistence_succeeded(true, true));
  EXPECT_FALSE(calibration_persistence_succeeded(true, false));
  EXPECT_FALSE(calibration_persistence_succeeded(false, true));
}

TEST(ATM90E32CalibrationPersistence, DoesNotAttemptSaveWhenWritesDoNotVerify) {
  bool save_called = false;
  const auto result = attempt_calibration_persistence(false, [&save_called] {
    save_called = true;
    return true;
  });

  EXPECT_FALSE(save_called);
  EXPECT_FALSE(result.attempted);
  EXPECT_FALSE(result.succeeded);
}

TEST(ATM90E32CalibrationPersistence, MarksFailedSaveAsAttempted) {
  bool save_called = false;
  const auto result = attempt_calibration_persistence(true, [&save_called] {
    save_called = true;
    return false;
  });

  EXPECT_TRUE(save_called);
  EXPECT_TRUE(result.attempted);
  EXPECT_FALSE(result.succeeded);
}

TEST(ATM90E32CalibrationRollback, VerifiesRegistersAfterRestoringValues) {
  int step = 0;
  bool restored = false;
  const bool succeeded = attempt_calibration_rollback(
      [&] {
        EXPECT_EQ(step++, 0);
        restored = true;
      },
      [&] {
        EXPECT_EQ(step++, 1);
        return restored && offset_register_value_matches(0xFF85, -123);
      });

  EXPECT_TRUE(succeeded);
  EXPECT_EQ(step, 2);
}

TEST(ATM90E32CalibrationRollback, FailsWhenRestoredRegisterReadbackMismatches) {
  bool restored = false;
  const bool succeeded = attempt_calibration_rollback(
      [&] { restored = true; }, [&] { return restored && offset_register_value_matches(0xFF84, -123); });

  EXPECT_FALSE(succeeded);
}

}  // namespace esphome::atm90e32::testing
