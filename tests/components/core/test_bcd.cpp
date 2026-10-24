#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {

static_assert(bcd_to_uint8(0x59) == 59);
static_assert(uint8_to_bcd(59) == 0x59);

TEST(Bcd, RoundTripsEveryTwoDigitValue) {
  for (uint8_t value = 0; value < 100; value++) {
    const uint8_t bcd = uint8_to_bcd(value);
    EXPECT_EQ(bcd >> 4, value / 10);
    EXPECT_EQ(bcd & 0x0F, value % 10);
    EXPECT_EQ(bcd_to_uint8(bcd), value);
  }
}

TEST(Bcd, DecodesBoundaryBytes) {
  EXPECT_EQ(bcd_to_uint8(0x00), 0);
  EXPECT_EQ(bcd_to_uint8(0x09), 9);
  EXPECT_EQ(bcd_to_uint8(0x10), 10);
  EXPECT_EQ(bcd_to_uint8(0x99), 99);
}

}  // namespace esphome::core::testing
