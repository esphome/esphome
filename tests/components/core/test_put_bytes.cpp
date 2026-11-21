#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

using Bytes = std::array<uint8_t, 6>;

// Writes at offset 1 into a marker-filled buffer so writes past the width show up.
template<typename T, typename V> constexpr Bytes put_at_1(uint8_t *(*put)(uint8_t *, T), V value) {
  Bytes buf{0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  put(buf.data() + 1, value);
  return buf;
}

// Usable in constant expressions.
static_assert(put_at_1(put_le32, 0x12345678) == Bytes{0xAA, 0x78, 0x56, 0x34, 0x12, 0xAA});

TEST(PutBytes, BigEndian) {
  EXPECT_EQ(put_at_1(put_be16, 0x1234), (Bytes{0xAA, 0x12, 0x34, 0xAA, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_be24, 0x123456), (Bytes{0xAA, 0x12, 0x34, 0x56, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_be32, 0x12345678), (Bytes{0xAA, 0x12, 0x34, 0x56, 0x78, 0xAA}));
}

TEST(PutBytes, LittleEndian) {
  EXPECT_EQ(put_at_1(put_le16, 0x1234), (Bytes{0xAA, 0x34, 0x12, 0xAA, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_le32, 0x12345678), (Bytes{0xAA, 0x78, 0x56, 0x34, 0x12, 0xAA}));
}

TEST(PutBytes, ReturnsEndOfWrite) {
  uint8_t buf[9]{};
  uint8_t *p = put_be16(buf, 0x0102);
  p = put_be24(p, 0x030405);
  p = put_le32(p, 0x09080706);
  EXPECT_EQ(p, buf + 9);
  for (uint8_t i = 0; i < 9; i++) {
    EXPECT_EQ(buf[i], i + 1);
  }
  EXPECT_EQ(put_le16(buf, 0), buf + 2);
  EXPECT_EQ(put_be32(buf, 0), buf + 4);
}

TEST(PutBytes, HighBitsAboveWidthAreDropped) {
  EXPECT_EQ(put_at_1(put_be24, 0xFF123456), (Bytes{0xAA, 0x12, 0x34, 0x56, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_be16, 0x12345), (Bytes{0xAA, 0x23, 0x45, 0xAA, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_le16, 0x12345), (Bytes{0xAA, 0x45, 0x23, 0xAA, 0xAA, 0xAA}));
}

TEST(PutBytes, SignedValuesUseTwosComplement) {
  EXPECT_EQ(put_at_1(put_be16, int16_t{-2}), (Bytes{0xAA, 0xFF, 0xFE, 0xAA, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_le16, int16_t{-2}), (Bytes{0xAA, 0xFE, 0xFF, 0xAA, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_be24, -2), (Bytes{0xAA, 0xFF, 0xFF, 0xFE, 0xAA, 0xAA}));
  EXPECT_EQ(put_at_1(put_be32, int32_t{-256}), (Bytes{0xAA, 0xFF, 0xFF, 0xFF, 0x00, 0xAA}));
  EXPECT_EQ(put_at_1(put_le32, int32_t{-256}), (Bytes{0xAA, 0x00, 0xFF, 0xFF, 0xFF, 0xAA}));
}

}  // namespace esphome::core::testing
