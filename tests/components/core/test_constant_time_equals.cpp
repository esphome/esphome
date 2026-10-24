#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome {

TEST(ConstantTimeEquals, EqualBuffers) {
  const uint8_t a[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
  const uint8_t b[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
  EXPECT_TRUE(constant_time_equals(a, b, sizeof(a)));
}

TEST(ConstantTimeEquals, FirstByteDiffers) {
  const uint8_t a[] = {0x00, 0x01, 0x02, 0x03};
  const uint8_t b[] = {0x10, 0x01, 0x02, 0x03};
  EXPECT_FALSE(constant_time_equals(a, b, sizeof(a)));
}

TEST(ConstantTimeEquals, LastByteDiffers) {
  const uint8_t a[] = {0x00, 0x01, 0x02, 0x03};
  const uint8_t b[] = {0x00, 0x01, 0x02, 0x13};
  EXPECT_FALSE(constant_time_equals(a, b, sizeof(a)));
}

TEST(ConstantTimeEquals, SingleBitDifference) {
  const uint8_t a[] = {0xAA, 0xAA};
  const uint8_t b[] = {0xAA, 0xAB};
  EXPECT_FALSE(constant_time_equals(a, b, sizeof(a)));
}

TEST(ConstantTimeEquals, ZeroLengthIsEqual) {
  const uint8_t a[] = {0x01};
  const uint8_t b[] = {0x02};
  EXPECT_TRUE(constant_time_equals(a, b, 0));
}

}  // namespace esphome
