#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1677_gray4.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"

namespace esphome::epaper_spi::testing {

/// Exposes the buffer and the dirty bounds so the fast path can be compared with drawing pixel by pixel.
template<typename Driver> class TestableDisplay : public Driver {
 public:
  TestableDisplay(uint16_t width, uint16_t height) : Driver("test", width, height, nullptr, 0) {
    EXPECT_TRUE(this->init_buffer_(this->buffer_length_));
    this->reset_bounds();
  }

  void reset_bounds() {
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }

  std::vector<uint8_t> buffer() const {
    std::vector<uint8_t> out;
    for (size_t i = 0; i != this->buffer_length_; i++)
      out.push_back(this->buffer_[i]);
    return out;
  }

  std::vector<uint16_t> bounds() const { return {this->x_low_, this->y_low_, this->x_high_, this->y_high_}; }

  void draw_one_by_one(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                       display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
    // The per-pixel path the fast one must match
    display::Display::draw_pixels_at(  // NOLINT(bugprone-parent-virtual-call)
        x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
  }
};

/// RGB565 data with a spread of light and dark pixels, including values either side of the threshold.
static std::vector<uint8_t> rgb565(size_t pixels, uint32_t seed) {
  std::vector<uint8_t> out;
  uint32_t state = seed;
  for (size_t i = 0; i != pixels; i++) {
    state = state * 1103515245u + 12345u;
    const uint16_t pixel = static_cast<uint16_t>(state >> 8);
    out.push_back(pixel & 0xFF);
    out.push_back(pixel >> 8);
  }
  return out;
}

/// For every rotation and transform, with source offsets, padding and a block that runs off the panel,
/// drawing a block directly gives the same buffer and the same dirty area as drawing it pixel by pixel.
TEST(EPaperDrawPixels, SameResultAsDrawingPixelByPixel) {
  const display::DisplayRotation rotations[] = {
      display::DISPLAY_ROTATION_0_DEGREES, display::DISPLAY_ROTATION_90_DEGREES, display::DISPLAY_ROTATION_180_DEGREES,
      display::DISPLAY_ROTATION_270_DEGREES};
  for (const auto rotation : rotations) {
    for (uint8_t transform = 0; transform != 8; transform++) {
      for (const bool big_endian : {false, true}) {
        TestableDisplay<EPaperSSD1683> fast(24, 16);
        TestableDisplay<EPaperSSD1683> slow(24, 16);
        for (auto *display : {&fast, &slow}) {
          display->set_transform(transform & ~SWAP_XY);
          display->set_rotation(rotation);
        }
        // a 20x12 block at (6, 5) with a 3 pixel left offset, 2 rows skipped and 4 pixels of padding
        const auto data = rgb565((3 + 20 + 4) * (2 + 12), rotation * 16 + transform);
        fast.draw_pixels_at(6, 5, 20, 12, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, big_endian,
                            3, 2, 4);
        slow.draw_one_by_one(6, 5, 20, 12, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565,
                             big_endian, 3, 2, 4);
        EXPECT_EQ(fast.buffer(), slow.buffer()) << "rotation " << rotation << " transform " << int(transform);
        EXPECT_EQ(fast.bounds(), slow.bounds()) << "rotation " << rotation << " transform " << int(transform);
      }
    }
  }
}

/// With a clipping region set the block goes through draw_pixel_at(), which honours it.
TEST(EPaperDrawPixels, ClippingFallsBackToDrawingPixelByPixel) {
  TestableDisplay<EPaperSSD1683> fast(24, 16);
  TestableDisplay<EPaperSSD1683> slow(24, 16);
  const auto data = rgb565(24 * 16, 7);
  for (auto *display : {&fast, &slow})
    display->start_clipping(4, 4, 12, 10);
  fast.draw_pixels_at(0, 0, 24, 16, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, false, 0, 0, 0);
  slow.draw_one_by_one(0, 0, 24, 16, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, false, 0, 0, 0);
  EXPECT_EQ(fast.buffer(), slow.buffer());
  EXPECT_EQ(fast.bounds(), (std::vector<uint16_t>{4, 4, 12, 10}));
}

/// A driver whose buffer is not 1 bit per pixel keeps its own draw_pixel_at() for blocks too.
TEST(EPaperDrawPixels, OtherBufferLayoutsDrawPixelByPixel) {
  TestableDisplay<EPaperSSD1677Gray4> fast(16, 4);
  TestableDisplay<EPaperSSD1677Gray4> slow(16, 4);
  const auto data = rgb565(16 * 4, 11);
  fast.draw_pixels_at(0, 0, 16, 4, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, false, 0, 0, 0);
  slow.draw_one_by_one(0, 0, 16, 4, data.data(), display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, false, 0, 0, 0);
  EXPECT_EQ(fast.buffer(), slow.buffer());
}

}  // namespace esphome::epaper_spi::testing
