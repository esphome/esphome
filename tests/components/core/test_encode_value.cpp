#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static constexpr uint8_t BYTES[] = {0x12, 0x34, 0x56, 0x78};

static_assert(encode_value_le<uint16_t>(BYTES) == 0x3412);
static_assert(encode_value<int16_t>(BYTES) == 0x1234);

TEST(EncodeValue, LittleEndianUnsigned) {
  EXPECT_EQ(encode_value_le<uint8_t>(BYTES), 0x12);
  EXPECT_EQ(encode_value_le<uint16_t>(BYTES + 1), 0x5634);
  EXPECT_EQ(encode_value_le<uint32_t>(BYTES), 0x78563412u);
}

TEST(EncodeValue, BigEndianSigned) {
  static constexpr uint8_t MINUS_ONE[] = {0xFF, 0xFF, 0xFF, 0xFF};
  static constexpr uint8_t MIN16[] = {0x80, 0x00};
  static constexpr uint8_t MIN32[] = {0x80, 0x00, 0x00, 0x00};
  EXPECT_EQ(encode_value<int8_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int16_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int32_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int16_t>(MIN16), INT16_MIN);
  EXPECT_EQ(encode_value<int32_t>(MIN32), INT32_MIN);
}

TEST(EncodeValue, LittleEndianSigned) {
  static constexpr uint8_t MIN16[] = {0x00, 0x80};
  static constexpr uint8_t MAX16[] = {0xFF, 0x7F};
  static constexpr uint8_t MINUS_TWO[] = {0xFE, 0xFF};
  static constexpr uint8_t MINUS_256[] = {0x00, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(encode_value_le<int16_t>(MIN16), INT16_MIN);
  EXPECT_EQ(encode_value_le<int16_t>(MAX16), INT16_MAX);
  EXPECT_EQ(encode_value_le<int16_t>(MINUS_TWO), -2);
  EXPECT_EQ(encode_value_le<int32_t>(MINUS_256), -256);
}

TEST(EncodeValue, BoolStillCompiles) {
  static constexpr uint8_t ONE[] = {0x01};
  static_assert(encode_value<bool>(ONE));
  static_assert(encode_value_le<bool>(ONE));
}

}  // namespace esphome::core::testing
