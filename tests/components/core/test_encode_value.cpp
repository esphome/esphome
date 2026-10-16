#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static constexpr uint8_t BYTES[] = {0x12, 0x34, 0x56, 0x78};

static_assert(encode_value<uint16_t>(BYTES) == 0x1234);
static_assert(encode_value_le<uint16_t>(BYTES) == 0x3412);
static_assert(encode_value_le<int16_t>(BYTES) == 0x3412);

TEST(EncodeValue, BigEndianUnsigned) {
  EXPECT_EQ(encode_value<uint8_t>(BYTES), 0x12);
  EXPECT_EQ(encode_value<uint16_t>(BYTES), 0x1234);
  EXPECT_EQ(encode_value<uint32_t>(BYTES), 0x12345678u);
  EXPECT_EQ(encode_value<uint16_t>(BYTES + 1), encode_uint16(0x34, 0x56));
  EXPECT_EQ(encode_value<uint32_t>(BYTES), encode_uint32(0x12, 0x34, 0x56, 0x78));
}

TEST(EncodeValue, LittleEndianUnsigned) {
  EXPECT_EQ(encode_value_le<uint8_t>(BYTES), 0x12);
  EXPECT_EQ(encode_value_le<uint16_t>(BYTES), 0x3412);
  EXPECT_EQ(encode_value_le<uint32_t>(BYTES), 0x78563412u);
  EXPECT_EQ(encode_value_le<uint16_t>(BYTES + 1), encode_uint16(0x56, 0x34));
  EXPECT_EQ(encode_value_le<uint32_t>(BYTES), encode_uint32(0x78, 0x56, 0x34, 0x12));
}

TEST(EncodeValue, BigEndianSigned) {
  static constexpr uint8_t MINUS_ONE[] = {0xFF, 0xFF, 0xFF, 0xFF};
  static constexpr uint8_t MIN16[] = {0x80, 0x00};
  static constexpr uint8_t MAX16[] = {0x7F, 0xFF};
  static constexpr uint8_t MINUS_TWO[] = {0xFF, 0xFE};
  static constexpr uint8_t MIN32[] = {0x80, 0x00, 0x00, 0x00};
  EXPECT_EQ(encode_value<int8_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int16_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int32_t>(MINUS_ONE), -1);
  EXPECT_EQ(encode_value<int16_t>(MIN16), INT16_MIN);
  EXPECT_EQ(encode_value<int16_t>(MAX16), INT16_MAX);
  EXPECT_EQ(encode_value<int16_t>(MINUS_TWO), -2);
  EXPECT_EQ(encode_value<int32_t>(MIN32), INT32_MIN);
  EXPECT_EQ(encode_value<int16_t>(MINUS_TWO), static_cast<int16_t>(encode_uint16(0xFF, 0xFE)));
}

TEST(EncodeValue, LittleEndianSigned) {
  static constexpr uint8_t MIN16[] = {0x00, 0x80};
  static constexpr uint8_t MAX16[] = {0xFF, 0x7F};
  static constexpr uint8_t MINUS_TWO[] = {0xFE, 0xFF};
  static constexpr uint8_t MIN32[] = {0x00, 0x00, 0x00, 0x80};
  static constexpr uint8_t MINUS_256[] = {0x00, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(encode_value_le<int16_t>(MIN16), INT16_MIN);
  EXPECT_EQ(encode_value_le<int16_t>(MAX16), INT16_MAX);
  EXPECT_EQ(encode_value_le<int16_t>(MINUS_TWO), -2);
  EXPECT_EQ(encode_value_le<int32_t>(MIN32), INT32_MIN);
  EXPECT_EQ(encode_value_le<int32_t>(MINUS_256), -256);
  EXPECT_EQ(encode_value_le<int16_t>(MINUS_TWO), static_cast<int16_t>(encode_uint16(0xFF, 0xFE)));
}

TEST(EncodeValue, StdArrayOverloads) {
  const std::array<uint8_t, 2> word{0x01, 0x80};
  EXPECT_EQ(encode_value<uint16_t>(word), 0x0180);
  EXPECT_EQ(encode_value_le<uint16_t>(word), 0x8001);
  EXPECT_EQ(encode_value<int16_t>(word), 0x0180);
  EXPECT_EQ(encode_value_le<int16_t>(word), -32767);
}

}  // namespace esphome::core::testing
