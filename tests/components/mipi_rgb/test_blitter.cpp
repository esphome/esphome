#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/mipi_rgb/mipi_rgb_blitter.h"

namespace esphome::mipi_rgb::testing {

// Reference rotation matching MipiRgb::rotate_() logic
static void reference_rotate(int &x, int &y, int width, int height, display::DisplayRotation rotation) {
  switch (rotation) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = width - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = width - x - 1;
      y = height - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = height - y - 1;
      break;
  }
}

static void test_rotation(display::DisplayRotation rotation) {
  constexpr int FB_WIDTH = 40;
  constexpr int FB_HEIGHT = 30;
  constexpr int W = 7, H = 5, X_OFFSET = 2, Y_OFFSET = 3, X_PAD = 4;
  constexpr size_t STRIDE = X_OFFSET + W + X_PAD;
  constexpr size_t LINES = Y_OFFSET + H;

  std::vector<uint16_t> source(STRIDE * LINES);
  uint16_t val = 0x1000;
  for (size_t y = 0; y < LINES; y++) {
    for (size_t x = 0; x < STRIDE; x++) {
      source[y * STRIDE + x] = val++;
    }
  }

  int x_start = 10;
  int y_start = 8;

  // Run blit_rotated_565
  std::vector<uint16_t> fb_actual(FB_WIDTH * FB_HEIGHT, 0);
  const uint16_t *src_origin = source.data() + (Y_OFFSET * STRIDE + X_OFFSET);
  blit_rotated_565(fb_actual.data(), FB_WIDTH, FB_HEIGHT, rotation, src_origin, x_start, y_start, W, H, STRIDE);

  // Compute reference per-pixel using reference_rotate
  std::vector<uint16_t> fb_expected(FB_WIDTH * FB_HEIGHT, 0);
  for (int py = 0; py < H; py++) {
    for (int px = 0; px < W; px++) {
      int rx = x_start + px;
      int ry = y_start + py;
      reference_rotate(rx, ry, FB_WIDTH, FB_HEIGHT, rotation);
      fb_expected[ry * FB_WIDTH + rx] = src_origin[py * STRIDE + px];
    }
  }

  EXPECT_EQ(fb_actual, fb_expected);
}

TEST(MipiRgbBlitter, Rotation0) { test_rotation(display::DISPLAY_ROTATION_0_DEGREES); }
TEST(MipiRgbBlitter, Rotation90) { test_rotation(display::DISPLAY_ROTATION_90_DEGREES); }
TEST(MipiRgbBlitter, Rotation180) { test_rotation(display::DISPLAY_ROTATION_180_DEGREES); }
TEST(MipiRgbBlitter, Rotation270) { test_rotation(display::DISPLAY_ROTATION_270_DEGREES); }

}  // namespace esphome::mipi_rgb::testing
