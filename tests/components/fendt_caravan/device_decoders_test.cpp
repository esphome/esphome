#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/fendt_caravan/device_decoders.h"

namespace esphome::fendt_caravan::testing {

TEST(FendtDecodeFloat, ReadsNumbers) {
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("21.5"), 21.5f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("21,5"), 21.5f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float(" -3 "), -3.0f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("0"), 0.0f);
}

TEST(FendtDecodeFloat, StripsCelsiusUnit) {
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float(" 20,0^C"), 20.0f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("-1,5 ^C"), -1.5f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("21,5\xC2\xB0"
                                               "C"),
                  21.5f);
  EXPECT_FLOAT_EQ(DeviceDecoders::decode_float("21,5\xB0"
                                               "C"),
                  21.5f);
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("^C")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("12C")));
}

TEST(FendtDecodeFloat, InvalidIsNan) {
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("  ")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("--")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("12abc")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("inf")));
  EXPECT_TRUE(std::isnan(DeviceDecoders::decode_float("1111111111111111111111111111111111111111")));
}

TEST(FendtDecodeInt, ReadsNumbers) {
  int out = -1;
  EXPECT_TRUE(DeviceDecoders::decode_int("0", out));
  EXPECT_EQ(out, 0);
  EXPECT_TRUE(DeviceDecoders::decode_int(" 2 ", out));
  EXPECT_EQ(out, 2);
  EXPECT_TRUE(DeviceDecoders::decode_int("-4", out));
  EXPECT_EQ(out, -4);
}

TEST(FendtDecodeInt, InvalidFailsAndKeepsOut) {
  int out = 7;
  EXPECT_FALSE(DeviceDecoders::decode_int("", out));
  EXPECT_FALSE(DeviceDecoders::decode_int(" ", out));
  EXPECT_FALSE(DeviceDecoders::decode_int("abc", out));
  EXPECT_FALSE(DeviceDecoders::decode_int("1.5", out));
  EXPECT_FALSE(DeviceDecoders::decode_int("99999999999999999999", out));
  EXPECT_EQ(out, 7);
}

}  // namespace esphome::fendt_caravan::testing
