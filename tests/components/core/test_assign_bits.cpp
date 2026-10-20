#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static_assert([] {
  uint8_t v = 0;
  assign_bits(v, 1 << 3, true);
  return v;
}() == 0x08);

TEST(AssignBits, SetsAndClearsOnlyTheMask) {
  uint8_t reg = 0b1010'0001;
  assign_bits(reg, 0b0000'0110, true);
  EXPECT_EQ(reg, 0b1010'0111);
  assign_bits(reg, 0b1000'0010, false);
  EXPECT_EQ(reg, 0b0010'0101);
}

TEST(AssignBits, ClearsAtTheTargetWidth) {
  uint16_t reg16 = 0xFFFF;
  assign_bits(reg16, 1 << 15, false);
  EXPECT_EQ(reg16, 0x7FFF);
  uint64_t reg64 = UINT64_MAX;
  assign_bits(reg64, uint32_t{1} << 31, false);
  EXPECT_EQ(reg64, UINT64_MAX & ~(uint64_t{1} << 31));
}

}  // namespace esphome::core::testing
