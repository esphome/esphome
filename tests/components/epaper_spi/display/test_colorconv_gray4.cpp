#include <gtest/gtest.h>

#include "esphome/components/epaper_spi/colorconv.h"

namespace esphome::epaper_spi::testing {
namespace {

// A gray Color whose rec601_luma is exactly its component value, so the boundary the caller
// asserts about is the luma passed to color_to_gray4.
Color gray(uint8_t value) { return Color(value, value, value); }

TEST(ColorToGray4, BoundaryValuesLandOnEachLevel) {
  EXPECT_EQ(color_to_gray4(gray(42)), 0);
  EXPECT_EQ(color_to_gray4(gray(43)), 1);
  EXPECT_EQ(color_to_gray4(gray(127)), 1);
  EXPECT_EQ(color_to_gray4(gray(128)), 2);
  EXPECT_EQ(color_to_gray4(gray(212)), 2);
  EXPECT_EQ(color_to_gray4(gray(213)), 3);
}

TEST(ColorToGray4, FullSwingExtremaMapToBlackAndWhite) {
  EXPECT_EQ(color_to_gray4(gray(0)), 0);
  EXPECT_EQ(color_to_gray4(gray(255)), 3);
}

// The quantized levels are equally spaced, so the midpoints between them land mid-band.
TEST(ColorToGray4, LevelCenterPointsMapToTheirBand) {
  EXPECT_EQ(color_to_gray4(gray(0)), 0);
  EXPECT_EQ(color_to_gray4(gray(85)), 1);
  EXPECT_EQ(color_to_gray4(gray(170)), 2);
  EXPECT_EQ(color_to_gray4(gray(255)), 3);
}

static_assert(color_to_gray4(Color(43, 43, 43)) == 1, "color_to_gray4 must stay usable in constexpr");
static_assert(color_to_gray4(Color(128, 128, 128)) == 2, "color_to_gray4 must stay usable in constexpr");

}  // namespace
}  // namespace esphome::epaper_spi::testing
