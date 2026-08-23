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

}  // namespace esphome::atm90e32::testing
