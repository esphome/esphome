#include <gtest/gtest.h>

#include "esphome/components/epaper_spi/colorconv.h"

namespace esphome::epaper_spi::testing {
namespace {

BwrColor to_bwr(Color color) {
  return color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);
}

TEST(ColorToBwr, PrimariesMapToThemselves) {
  EXPECT_EQ(to_bwr(Color(0, 0, 0)), BwrColor::BWR_COLOR_BLACK);
  EXPECT_EQ(to_bwr(Color(255, 255, 255)), BwrColor::BWR_COLOR_WHITE);
  EXPECT_EQ(to_bwr(Color(255, 0, 0)), BwrColor::BWR_COLOR_RED);
}

TEST(ColorToBwr, GraysMapToBlackOrWhiteByLuma) {
  EXPECT_EQ(to_bwr(Color(100, 100, 100)), BwrColor::BWR_COLOR_BLACK);
  EXPECT_EQ(to_bwr(Color(200, 200, 200)), BwrColor::BWR_COLOR_WHITE);
}

TEST(ColorToBwr, OrangesMapToRed) {
  EXPECT_EQ(to_bwr(Color(255, 165, 0)), BwrColor::BWR_COLOR_RED);  // named orange
  EXPECT_EQ(to_bwr(Color(255, 100, 0)), BwrColor::BWR_COLOR_RED);
  EXPECT_EQ(to_bwr(Color(255, 69, 0)), BwrColor::BWR_COLOR_RED);  // orange red
}

TEST(ColorToBwr, MutedAndDarkRedsMapToRed) {
  EXPECT_EQ(to_bwr(Color(200, 100, 100)), BwrColor::BWR_COLOR_RED);
  EXPECT_EQ(to_bwr(Color(128, 0, 0)), BwrColor::BWR_COLOR_RED);  // maroon
}

TEST(ColorToBwr, VeryDarkRedsMapToBlack) { EXPECT_EQ(to_bwr(Color(100, 0, 0)), BwrColor::BWR_COLOR_BLACK); }

TEST(ColorToBwr, PaleColorsMapToWhite) {
  EXPECT_EQ(to_bwr(Color(255, 129, 129)), BwrColor::BWR_COLOR_WHITE);  // pale pink
  EXPECT_EQ(to_bwr(Color(0, 255, 255)), BwrColor::BWR_COLOR_WHITE);    // cyan
}

TEST(ColorToBwr, TiesGoToWhite) {
  EXPECT_EQ(to_bwr(Color(255, 255, 0)), BwrColor::BWR_COLOR_WHITE);  // yellow: equidistant from red and white
}

TEST(ColorToBwr, DarkColorsMapToBlack) {
  EXPECT_EQ(to_bwr(Color(0, 0, 255)), BwrColor::BWR_COLOR_BLACK);  // blue
  EXPECT_EQ(to_bwr(Color(0, 128, 0)), BwrColor::BWR_COLOR_BLACK);  // dark green
}

static_assert(color_to_bwr<int>(Color(255, 165, 0), 0, 1, 2) == 2, "color_to_bwr must stay usable in constexpr");

}  // namespace
}  // namespace esphome::epaper_spi::testing
