#include <gtest/gtest.h>

#include "esphome/components/epaper_spi/colorconv.h"

namespace esphome::epaper_spi::testing {
namespace {

uint8_t gray4(uint8_t value) { return color_to_gray4(Color(value, value, value)); }

// Four evenly spaced greys, as a 2 bits per pixel font or image blends them, are the four levels
TEST(ColorToGray4, EvenlySpacedGraysMapToTheirLevels) {
  EXPECT_EQ(gray4(0), 0);
  EXPECT_EQ(gray4(85), 1);
  EXPECT_EQ(gray4(170), 2);
  EXPECT_EQ(gray4(255), 3);
}

TEST(ColorToGray4, OtherGraysMapToTheNearestLevel) {
  EXPECT_EQ(gray4(42), 0);
  EXPECT_EQ(gray4(43), 1);
  EXPECT_EQ(gray4(127), 1);
  EXPECT_EQ(gray4(128), 2);
  EXPECT_EQ(gray4(212), 2);
  EXPECT_EQ(gray4(213), 3);
}

// A black-and-white partial update shows the high bit of each level, so it must be light or dark
// as a monochrome model would make the same colour
TEST(ColorToGray4, HighBitMatchesMonochrome) {
  for (int value = 0; value <= 255; value++) {
    EXPECT_EQ(gray4((uint8_t) value) >> 1, color_to_mono(Color(value, value, value))) << "gray " << value;
  }
}

static_assert(color_to_gray4(Color(170, 170, 170)) == 2, "color_to_gray4 must stay usable in constexpr");

}  // namespace
}  // namespace esphome::epaper_spi::testing
