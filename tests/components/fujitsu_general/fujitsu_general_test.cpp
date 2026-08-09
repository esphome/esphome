#include <gtest/gtest.h>
#include "esphome/components/fujitsu_general/fujitsu_general.h"

namespace esphome::fujitsu_general::testing {

// The mode field of a received frame is three bits wide. The fourth bit of the same nibble belongs
// to the clean feature, so it has to be ignored when reading the mode.

TEST(FujitsuGeneralDecodeModeTest, DecodesTheAssignedModes) {
  EXPECT_EQ(decode_mode(0x00, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT_COOL);
  EXPECT_EQ(decode_mode(0x01, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decode_mode(0x02, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_DRY);
  EXPECT_EQ(decode_mode(0x03, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_EQ(decode_mode(0x04, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT);
}

TEST(FujitsuGeneralDecodeModeTest, IgnoresTheCleanBit) {
  // 0x0B is fan mode with the clean bit set. It used to be read as one value and reported as
  // heat/cool, which is the bug this covers.
  EXPECT_EQ(decode_mode(0x0B, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_FAN_ONLY);

  EXPECT_EQ(decode_mode(0x08, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT_COOL);
  EXPECT_EQ(decode_mode(0x09, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decode_mode(0x0A, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_DRY);
  EXPECT_EQ(decode_mode(0x0C, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT);
}

TEST(FujitsuGeneralDecodeModeTest, KeepsTheCurrentModeForUnassignedValues) {
  // 0x5 to 0x7 fit in the field but the protocol does not use them.
  EXPECT_EQ(decode_mode(0x05, climate::CLIMATE_MODE_COOL), climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decode_mode(0x06, climate::CLIMATE_MODE_HEAT), climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(decode_mode(0x07, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_OFF);
}

// The fan speed field is three bits wide as well, and used to fold every value it did not
// recognise into the automatic speed.

TEST(FujitsuGeneralDecodeFanModeTest, DecodesTheAssignedSpeeds) {
  EXPECT_EQ(decode_fan_mode(0x00, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(decode_fan_mode(0x01, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x02, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_MEDIUM);
  EXPECT_EQ(decode_fan_mode(0x03, climate::CLIMATE_FAN_AUTO), climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(decode_fan_mode(0x04, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_QUIET);
}

TEST(FujitsuGeneralDecodeFanModeTest, IgnoresTheFourthBit) {
  EXPECT_EQ(decode_fan_mode(0x0C, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_QUIET);
}

TEST(FujitsuGeneralDecodeFanModeTest, KeepsTheCurrentFanModeForUnassignedValues) {
  EXPECT_EQ(decode_fan_mode(0x05, climate::CLIMATE_FAN_HIGH), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x07, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_LOW);
}

TEST(FujitsuGeneralDecodeFanModeTest, LeavesAnUnsetFanModeUnset) {
  EXPECT_FALSE(decode_fan_mode(0x05, {}).has_value());
}

}  // namespace esphome::fujitsu_general::testing
