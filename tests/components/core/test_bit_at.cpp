#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static_assert(bit_at(uint8_t{0b1010'0001}, 0));
static_assert(!bit_at(uint8_t{0b1010'0001}, 1));
static_assert(bit_at(uint8_t{0b1010'0001}, 7));
static_assert(bit_at(uint64_t{1} << 63, 63));
static_assert(!bit_at(uint64_t{1} << 63, 62));
static_assert(bit_at(int32_t{-1}, 31));
static_assert(!bit_at(int32_t{0x7FFF'FFFF}, 31));

TEST(BitAt, IsTheReadSideOfAssignBits) {
  uint16_t reg = 0;
  assign_bits(reg, 1 << 9, true);
  EXPECT_TRUE(bit_at(reg, 9));
  assign_bits(reg, 1 << 9, false);
  EXPECT_FALSE(bit_at(reg, 9));
}

}  // namespace esphome::core::testing
