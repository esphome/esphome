#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/mipi_rgb/mipi_rgb_blitter.h"

namespace esphome::mipi_rgb::testing {

static void test_rotation(display::DisplayRotation rotation) {
  constexpr int fb_width = 40;
  constexpr int fb_height = 30;
  constexpr int w = 7, h = 5, x_offset = 2, y_offset = 3, x_pad = 4;
  constexpr size_t stride = x_offset + w + x_pad;
  constexpr size_t lines = y_offset + h;

  std::vector<uint16_t> source(stride * lines);
  uint16_t val = 0x1000;
  for (size_t y = 0; y < lines; y++) {
    for (size_t x = 0; x < stride; x++) {
      source[y * stride + x] = val++;
    }
  }

  int x_start = 10;
  int y_start = 8;

  // Run blit_rotated_565
  std::vector<uint16_t> fb_actual(fb_width * fb_height, 0);
  const uint16_t *src_origin = source.data() + (y_offset * stride + x_offset);
  blit_rotated_565(fb_actual.data(), fb_width, fb_height, rotation, src_origin, x_start, y_start, w, h, stride);

  // Compute reference per-pixel using rotate_point
  std::vector<uint16_t> fb_expected(fb_width * fb_height, 0);
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      int rx = x_start + px;
      int ry = y_start + py;
      rotate_point(rx, ry, fb_width, fb_height, rotation);
      fb_expected[ry * fb_width + rx] = src_origin[py * stride + px];
    }
  }

  EXPECT_EQ(fb_actual, fb_expected);
}

TEST(MipiRgbBlitter, Rotation0) { test_rotation(display::DISPLAY_ROTATION_0_DEGREES); }
TEST(MipiRgbBlitter, Rotation90) { test_rotation(display::DISPLAY_ROTATION_90_DEGREES); }
TEST(MipiRgbBlitter, Rotation180) { test_rotation(display::DISPLAY_ROTATION_180_DEGREES); }
TEST(MipiRgbBlitter, Rotation270) { test_rotation(display::DISPLAY_ROTATION_270_DEGREES); }

}  // namespace esphome::mipi_rgb::testing
