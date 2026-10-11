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
  EXPECT_EQ(decode_mode(0x07, climate::CLIMATE_MODE_DRY), climate::CLIMATE_MODE_DRY);

  // The same three with the clean bit set. Without the mask these would not reach this branch.
  EXPECT_EQ(decode_mode(0x0D, climate::CLIMATE_MODE_COOL), climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decode_mode(0x0E, climate::CLIMATE_MODE_HEAT), climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(decode_mode(0x0F, climate::CLIMATE_MODE_FAN_ONLY), climate::CLIMATE_MODE_FAN_ONLY);
}

TEST(FujitsuGeneralDecodeModeTest, NeverReportsOffForAStateFrame) {
  // A state frame describes a running unit, so keeping an off current mode would publish it as off
  // and turn the next transmission into a power off command. Automatic is the least specific mode
  // available, which is what the field's unassigned values decoded to before they were masked.
  for (uint8_t field = 0x05; field <= 0x07; field++) {
    SCOPED_TRACE(static_cast<int>(field));
    EXPECT_EQ(decode_mode(field, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT_COOL);
    EXPECT_EQ(decode_mode(field | 0b1000, climate::CLIMATE_MODE_OFF), climate::CLIMATE_MODE_HEAT_COOL);
  }
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
  EXPECT_EQ(decode_fan_mode(0x08, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(decode_fan_mode(0x09, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x0A, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_MEDIUM);
  EXPECT_EQ(decode_fan_mode(0x0B, climate::CLIMATE_FAN_AUTO), climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(decode_fan_mode(0x0C, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_QUIET);
}

TEST(FujitsuGeneralDecodeFanModeTest, KeepsTheCurrentFanModeForUnassignedValues) {
  EXPECT_EQ(decode_fan_mode(0x05, climate::CLIMATE_FAN_HIGH), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x06, climate::CLIMATE_FAN_MEDIUM), climate::CLIMATE_FAN_MEDIUM);
  EXPECT_EQ(decode_fan_mode(0x07, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(decode_fan_mode(0x0D, climate::CLIMATE_FAN_HIGH), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x0E, climate::CLIMATE_FAN_HIGH), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(decode_fan_mode(0x0F, climate::CLIMATE_FAN_LOW), climate::CLIMATE_FAN_LOW);
}

TEST(FujitsuGeneralDecodeFanModeTest, LeavesAnUnsetFanModeUnset) {
  EXPECT_FALSE(decode_fan_mode(0x05, {}).has_value());
}

// The swing field is only two bits wide. The two bits above it are reserved, and were read as part
// of the value.

TEST(FujitsuGeneralDecodeSwingModeTest, DecodesTheAssignedValues) {
  EXPECT_EQ(decode_swing_mode(0x00), climate::CLIMATE_SWING_OFF);
  EXPECT_EQ(decode_swing_mode(0x01), climate::CLIMATE_SWING_VERTICAL);
  EXPECT_EQ(decode_swing_mode(0x02), climate::CLIMATE_SWING_HORIZONTAL);
  EXPECT_EQ(decode_swing_mode(0x03), climate::CLIMATE_SWING_BOTH);
}

TEST(FujitsuGeneralDecodeSwingModeTest, IgnoresTheReservedBits) {
  // Without the mask everything from 0x04 up fell through to the default branch and reported swing
  // off. All twelve are covered, so the field's whole input space is asserted.
  const climate::ClimateSwingMode expected[] = {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                                climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH};
  for (uint8_t field = 0x04; field <= 0x0F; field++) {
    SCOPED_TRACE(static_cast<int>(field));
    EXPECT_EQ(decode_swing_mode(field), expected[field & 0b0011]);
  }
}

// Every state frame annotated in fujitsu_general.h, as the bytes those rows spell out. None of them
// sets the fourth bit of the mode or fan field, or either bit above the swing field, so the masks
// must leave all of them decoding exactly as they did before this change.

namespace {

struct CapturedFrame {
  const char *label;
  uint8_t bytes[16];
  uint8_t temperature;
  bool turn_on;
  climate::ClimateMode mode;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
};

constexpr CapturedFrame CAPTURED_FRAMES[] = {
    {"auto auto 18",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x8F},
     18,
     true,
     climate::CLIMATE_MODE_HEAT_COOL,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"auto auto 19",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x7F},
     19,
     true,
     climate::CLIMATE_MODE_HEAT_COOL,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"auto auto 30 (temperatures)",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCF},
     30,
     true,
     climate::CLIMATE_MODE_HEAT_COOL,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"on at 16",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x20, 0xAB},
     16,
     true,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"down to 16",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x20, 0xAC},
     16,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"auto auto 30 (mode options)",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCF},
     30,
     true,
     climate::CLIMATE_MODE_HEAT_COOL,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"cool auto 30",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x01, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCE},
     30,
     true,
     climate::CLIMATE_MODE_COOL,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"dry auto 30",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x02, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCD},
     30,
     true,
     climate::CLIMATE_MODE_DRY,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"fan (auto) (30)",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x03, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCC},
     30,
     true,
     climate::CLIMATE_MODE_FAN_ONLY,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"heat auto 30",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x04, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCB},
     30,
     true,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_AUTO,
     climate::CLIMATE_SWING_OFF},
    {"heat 30 high",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE1, 0x04, 0x01, 0x00, 0x00, 0x00, 0x20, 0xCA},
     30,
     true,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_HIGH,
     climate::CLIMATE_SWING_OFF},
    {"heat 30 med",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE0, 0x04, 0x02, 0x00, 0x00, 0x00, 0x20, 0xCA},
     30,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_MEDIUM,
     climate::CLIMATE_SWING_OFF},
    {"heat 30 low",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE0, 0x04, 0x03, 0x00, 0x00, 0x00, 0x20, 0xC9},
     30,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_LOW,
     climate::CLIMATE_SWING_OFF},
    {"heat 30 quiet",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE0, 0x04, 0x04, 0x00, 0x00, 0x00, 0x20, 0xC8},
     30,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_QUIET,
     climate::CLIMATE_SWING_OFF},
    {"heat 30 swing vert",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE0, 0x04, 0x14, 0x00, 0x00, 0x00, 0x20, 0xB8},
     30,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_QUIET,
     climate::CLIMATE_SWING_VERTICAL},
    {"heat 30 noswing",
     {0x14, 0x63, 0x00, 0x10, 0x10, 0xFE, 0x09, 0x30, 0xE0, 0x04, 0x04, 0x00, 0x00, 0x00, 0x20, 0xC8},
     30,
     false,
     climate::CLIMATE_MODE_HEAT,
     climate::CLIMATE_FAN_QUIET,
     climate::CLIMATE_SWING_OFF},
};

}  // namespace

TEST(FujitsuGeneralCaptureTest, DecodesEveryCapturedFrame) {
  for (const auto &frame : CAPTURED_FRAMES) {
    SCOPED_TRACE(frame.label);
    // Read through the component's own nibble helper and field indices, so this also fails if the
    // frame layout the header records ever stops matching what on_receive() reads.
    EXPECT_EQ(get_nibble(frame.bytes, FUJITSU_GENERAL_TEMPERATURE_NIBBLE) + FUJITSU_GENERAL_TEMP_MIN,
              frame.temperature);
    // The turn on flag is only written by transmit_state(), so this pins the frame layout rather
    // than a decode path.
    EXPECT_EQ(get_nibble(frame.bytes, FUJITSU_GENERAL_POWER_ON_NIBBLE) != 0, frame.turn_on);
    EXPECT_EQ(decode_mode(get_nibble(frame.bytes, FUJITSU_GENERAL_MODE_NIBBLE), climate::CLIMATE_MODE_OFF), frame.mode);
    EXPECT_EQ(decode_fan_mode(get_nibble(frame.bytes, FUJITSU_GENERAL_FAN_NIBBLE), climate::CLIMATE_FAN_ON),
              frame.fan_mode);
    EXPECT_EQ(decode_swing_mode(get_nibble(frame.bytes, FUJITSU_GENERAL_SWING_NIBBLE)), frame.swing_mode);
  }
}

}  // namespace esphome::fujitsu_general::testing
