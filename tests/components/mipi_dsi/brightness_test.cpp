#include <gtest/gtest.h>

#include "esphome/components/mipi_dsi/brightness.h"

namespace esphome::mipi_dsi::testing {

TEST(MipiDsiBrightness, ScalesBrightnessIntoTheRange) {
  EXPECT_EQ(scale_brightness(0.0f, 10, 200), 10);
  EXPECT_EQ(scale_brightness(0.5f, 10, 200), 105);
  EXPECT_EQ(scale_brightness(1.0f, 10, 200), 200);
}

TEST(MipiDsiBrightness, ScalesTheDefaultRange) {
  EXPECT_EQ(scale_brightness(0.0f, 0, 255), 0);
  EXPECT_EQ(scale_brightness(0.25f, 0, 255), 64);
  EXPECT_EQ(scale_brightness(1.0f, 0, 255), 255);
}

}  // namespace esphome::mipi_dsi::testing
