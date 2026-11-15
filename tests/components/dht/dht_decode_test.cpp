#include <gtest/gtest.h>

#include "esphome/components/dht/dht.h"

namespace esphome::dht::testing {

TEST(DHTDecodeTemperature, Positive) {
  EXPECT_EQ(decode_dht22_temperature(0x0000), 0);
  EXPECT_EQ(decode_dht22_temperature(0x00E7), 231);  // 23.1 C
  EXPECT_EQ(decode_dht22_temperature(0x01F4), 500);  // 50.0 C
}

TEST(DHTDecodeTemperature, SignAndMagnitude) {
  EXPECT_EQ(decode_dht22_temperature(0x8034), -52);   // -5.2 C, read as -5.3 C before
  EXPECT_EQ(decode_dht22_temperature(0x8001), -1);    // -0.1 C
  EXPECT_EQ(decode_dht22_temperature(0x8000), 0);     // -0.0 C
  EXPECT_EQ(decode_dht22_temperature(0x8190), -400);  // -40.0 C
}

TEST(DHTDecodeTemperature, SixteenBitTwosComplement) {
  EXPECT_EQ(decode_dht22_temperature(0xFFCC), -52);   // -5.2 C
  EXPECT_EQ(decode_dht22_temperature(0xFE70), -400);  // -40.0 C
}

TEST(DHTDecodeTemperature, TwelveBitTwosComplement) {
  EXPECT_EQ(decode_dht22_temperature(0x0FCC), -52);   // -5.2 C
  EXPECT_EQ(decode_dht22_temperature(0x0E70), -400);  // -40.0 C
}

}  // namespace esphome::dht::testing
