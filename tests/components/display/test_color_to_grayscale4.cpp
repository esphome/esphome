#include <gtest/gtest.h>

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"

namespace esphome::display::testing {

static uint32_t gray4(Color color) { return ColorUtil::color_to_grayscale4(color); }

TEST(ColorToGrayscale4, OnAndOff) {
  EXPECT_EQ(gray4(COLOR_ON), 15u);
  EXPECT_EQ(gray4(COLOR_OFF), 0u);
}

// Colors defined with only the white channel, as the grayscale display docs describe.
TEST(ColorToGrayscale4, WhiteOnlyColors) {
  EXPECT_EQ(gray4(Color(0, 0, 0, 255)), 15u);
  EXPECT_EQ(gray4(Color(0, 0, 0, 128)), 8u);
  EXPECT_EQ(gray4(Color(0, 0, 0, 0)), 0u);
}

// LVGL and other RGB sources leave the white channel at 0.
TEST(ColorToGrayscale4, RgbColorsWithoutWhite) {
  EXPECT_EQ(gray4(Color(255, 255, 255, 0)), 15u);
  EXPECT_EQ(gray4(Color(128, 128, 128, 0)), 8u);
  EXPECT_EQ(gray4(Color(0, 255, 0, 0)), 9u);  // luma 150
  EXPECT_EQ(gray4(Color(0, 0, 255, 0)), 1u);  // luma 29
}

// Font::print() blends all four channels; an edge pixel of COLOR_ON text on a white-only gray background.
TEST(ColorToGrayscale4, BlendOfWhiteOnlyAndRgb) { EXPECT_EQ(gray4(Color(17, 17, 17, 136)), 8u); }

}  // namespace esphome::display::testing
