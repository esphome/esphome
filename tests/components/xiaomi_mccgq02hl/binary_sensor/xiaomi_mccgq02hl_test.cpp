#include "../common.h"

#include <gtest/gtest.h>

namespace esphome::xiaomi_mccgq02hl::testing {

TEST(XiaomiMCCGQ02HL, DecodesOpenAndClosed) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, OPEN)));
  EXPECT_TRUE(h.sensor.state);
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, CLOSED)));
  EXPECT_FALSE(h.sensor.state);
}

TEST(XiaomiMCCGQ02HL, LeftOpenIsReportedAsOpen) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, LEFT_OPEN)));
  EXPECT_TRUE(h.sensor.state);
}

TEST(XiaomiMCCGQ02HL, DeviceResetIsIgnored) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, OPEN)));
  EXPECT_FALSE(h.sensor.parse_device(advert(ADDRESS, DEVICE_RESET)));
  EXPECT_TRUE(h.sensor.state);
}

TEST(XiaomiMCCGQ02HL, DecodesLight) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, BRIGHT)));
  EXPECT_TRUE(h.light.state);
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, DARK)));
  EXPECT_FALSE(h.light.state);
  EXPECT_EQ(h.door_changes, 0);  // light frames carry no door state
}

TEST(XiaomiMCCGQ02HL, DecodesBatteryLevel) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, BATTERY)));
  EXPECT_FLOAT_EQ(h.battery_level.state, 87.0f);
}

TEST(XiaomiMCCGQ02HL, RepeatedFrameIsDropped) {
  Harness h;
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, OPEN)));
  EXPECT_FALSE(h.sensor.parse_device(advert(ADDRESS, OPEN)));
}

TEST(XiaomiMCCGQ02HL, RejectsBadMic) {
  Harness h;
  EXPECT_FALSE(h.sensor.parse_device(advert(ADDRESS, BAD_MIC)));
  EXPECT_EQ(h.door_changes, 0);
}

TEST(XiaomiMCCGQ02HL, BadMicDoesNotBlockTheNextFrame) {
  Harness h;
  EXPECT_FALSE(h.sensor.parse_device(advert(ADDRESS, BAD_MIC)));
  ASSERT_TRUE(h.sensor.parse_device(advert(ADDRESS, OPEN)));
  EXPECT_TRUE(h.sensor.state);
}

TEST(XiaomiMCCGQ02HL, RejectsPlaintextFrame) {
  Harness h;
  EXPECT_FALSE(h.sensor.parse_device(advert(ADDRESS, PLAINTEXT_OPEN)));
  EXPECT_EQ(h.door_changes, 0);
}

TEST(XiaomiMCCGQ02HL, IgnoresOtherAddress) {
  Harness h;
  EXPECT_FALSE(h.sensor.parse_device(advert(OTHER_ADDRESS, OPEN)));
  EXPECT_EQ(h.door_changes, 0);
}

}  // namespace esphome::xiaomi_mccgq02hl::testing
