#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static constexpr uint8_t BIT_3 = 1 << 3;

static constexpr uint8_t assigned(uint8_t value, uint8_t mask, bool on) {
  assign_bits(value, mask, on);
  return value;
}

static_assert(assigned(0x00, BIT_3, true) == 0x08);
static_assert(assigned(0xFF, BIT_3, false) == 0xF7);

TEST(AssignBits, SetsAndClearsOnlyTheMask) {
  uint8_t reg = 0b1010'0001;
  assign_bits(reg, 0b0000'0110, true);
  EXPECT_EQ(reg, 0b1010'0111);
  assign_bits(reg, 0b1000'0010, false);
  EXPECT_EQ(reg, 0b0010'0101);
}

TEST(AssignBits, KeepsTheTargetWidth) {
  uint16_t reg = 0xFFFF;
  assign_bits(reg, 1u << 15, false);
  EXPECT_EQ(reg, 0x7FFF);
  uint32_t wide = 0;
  assign_bits(wide, 1ul << 31, true);
  EXPECT_EQ(wide, 0x80000000u);
}

}  // namespace esphome::core::testing
