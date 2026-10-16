#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/colorconv.h"
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

  /// Reallocates the buffer in blocks of at most max_block bytes, as SplitBuffer does when memory is fragmented.
  void split_buffer(size_t max_block) {
    ASSERT_TRUE(this->buffer_.init(this->buffer_length_, max_block));
    ASSERT_GT(this->buffer_.get_buffer_count(), 1u);
    this->clear();
    this->reset_bounds();
  }

  std::vector<uint16_t> bounds() const { return {this->x_low_, this->y_low_, this->x_high_, this->y_high_}; }

  void draw_one_by_one(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorBitness bitness,
                       bool big_endian, int x_offset, int y_offset, int x_pad) {
    // The per-pixel path the fast one must match
    display::Display::draw_pixels_at(  // NOLINT(bugprone-parent-virtual-call)
        x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
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

/// Random bytes for a block of pixels, enough for 3 bytes a pixel.
static std::vector<uint8_t> random_bytes(size_t pixels, uint32_t seed) {
  std::vector<uint8_t> out;
  uint32_t state = seed;
  for (size_t i = 0; i != pixels * 3; i++) {
    state = state * 1103515245u + 12345u;
    out.push_back(static_cast<uint8_t>(state >> 16));
  }
  return out;
}

/// For every rotation, mirroring and byte order, with source offsets and padding, and with blocks inside the panel,
/// running off each edge and larger than the panel, drawing an RGB565 block directly gives the same buffer and the same
/// dirty area as drawing it pixel by pixel. The panel width is not a multiple of 8.
TEST(EPaperDrawPixels, SameResultAsDrawingPixelByPixel) {
  struct Block {
    int x, y, w, h;
  };
  const Block blocks[] = {{6, 5, 12, 6}, {-3, -2, 10, 8}, {14, 8, 12, 9}, {-5, 4, 30, 3}, {-2, -3, 26, 20}};
  const display::DisplayRotation rotations[] = {
      display::DISPLAY_ROTATION_0_DEGREES, display::DISPLAY_ROTATION_90_DEGREES, display::DISPLAY_ROTATION_180_DEGREES,
      display::DISPLAY_ROTATION_270_DEGREES};
  // Swapping the axes comes from the 90 and 270 degree rotations
  const uint8_t mirrorings[] = {NONE, MIRROR_X, MIRROR_Y, MIRROR_X | MIRROR_Y};
  uint32_t seed = 1;
  for (const auto rotation : rotations) {
    for (const uint8_t mirroring : mirrorings) {
      for (const bool big_endian : {false, true}) {
        for (const auto &block : blocks) {
          TestableDisplay<EPaperSSD1683> fast(21, 13);
          TestableDisplay<EPaperSSD1683> slow(21, 13);
          for (auto *display : {&fast, &slow}) {
            display->set_transform(mirroring);
            display->set_rotation(rotation);
          }
          // a 3 pixel left offset, 2 rows skipped and 4 pixels of padding
          const auto data = random_bytes((3 + block.w + 4) * (2 + block.h), seed++);
          fast.draw_pixels_at(block.x, block.y, block.w, block.h, data.data(), display::COLOR_BITNESS_565, big_endian,
                              3, 2, 4);
          slow.draw_one_by_one(block.x, block.y, block.w, block.h, data.data(), display::COLOR_BITNESS_565, big_endian,
                               3, 2, 4);
          const auto where = ::testing::Message() << "rotation " << rotation << " mirroring " << int(mirroring)
                                                  << " big endian " << big_endian << " block " << block.x << ","
                                                  << block.y << " " << block.w << "x" << block.h;
          EXPECT_EQ(fast.buffer(), slow.buffer()) << where;
          EXPECT_EQ(fast.bounds(), slow.bounds()) << where;
        }
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
  fast.draw_pixels_at(0, 0, 24, 16, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
  slow.draw_one_by_one(0, 0, 24, 16, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
  EXPECT_EQ(fast.buffer(), slow.buffer());
  EXPECT_EQ(fast.bounds(), (std::vector<uint16_t>{4, 4, 12, 10}));
}

/// A driver whose buffer is not 1 bit per pixel keeps its own draw_pixel_at() for blocks too.
TEST(EPaperDrawPixels, OtherBufferLayoutsDrawPixelByPixel) {
  TestableDisplay<EPaperSSD1677Gray4> fast(16, 4);
  TestableDisplay<EPaperSSD1677Gray4> slow(16, 4);
  const auto data = rgb565(16 * 4, 11);
  fast.draw_pixels_at(0, 0, 16, 4, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
  slow.draw_one_by_one(0, 0, 16, 4, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
  EXPECT_EQ(fast.buffer(), slow.buffer());
}

/// The RGB565 lookup gives exactly what converting the color and thresholding it does, for every value.
TEST(EPaperDrawPixels, Rgb565LookupMatchesTheColorConversion) {
  const Rgb565ToMono to_mono;
  size_t mismatches = 0;
  for (uint32_t value = 0; value != 0x10000; value++) {
    const uint8_t expected = color_to_mono(ColorUtil::to_color(value, display::COLOR_BITNESS_565));
    if (to_mono(static_cast<uint16_t>(value)) != expected)
      mismatches++;
  }
  EXPECT_EQ(mismatches, 0u);
}

// Remove before 2027.5.0
/// The deprecated lookup that takes a color order still matches the deprecated conversion with that order.
TEST(EPaperDrawPixels, DeprecatedOrderedLookupMatchesTheColorConversion) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  for (const auto order : {display::COLOR_ORDER_RGB, display::COLOR_ORDER_BGR, display::COLOR_ORDER_GRB}) {
    const Rgb565ToMono to_mono(order);
    size_t mismatches = 0;
    for (uint32_t value = 0; value != 0x10000; value++) {
      const uint8_t expected = color_to_mono(ColorUtil::to_color(value, order, display::COLOR_BITNESS_565));
      if (to_mono(static_cast<uint16_t>(value)) != expected)
        mismatches++;
    }
    EXPECT_EQ(mismatches, 0u) << "order " << int(order);
  }
#pragma GCC diagnostic pop
}

/// Other bit depths, and a buffer split into several blocks, go through draw_pixel_at() and give the same result.
TEST(EPaperDrawPixels, OtherCasesDrawPixelByPixel) {
  {
    TestableDisplay<EPaperSSD1683> fast(24, 16);
    TestableDisplay<EPaperSSD1683> slow(24, 16);
    fast.split_buffer(7);
    slow.split_buffer(7);
    const auto data = rgb565(24 * 16, 5);
    fast.draw_pixels_at(-2, 3, 24, 16, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
    slow.draw_one_by_one(-2, 3, 24, 16, data.data(), display::COLOR_BITNESS_565, false, 0, 0, 0);
    EXPECT_EQ(fast.buffer(), slow.buffer()) << "split buffer";
    EXPECT_EQ(fast.bounds(), slow.bounds()) << "split buffer";
  }
  for (const auto bitness : {display::COLOR_BITNESS_888, display::COLOR_BITNESS_332}) {
    TestableDisplay<EPaperSSD1683> fast(24, 16);
    TestableDisplay<EPaperSSD1683> slow(24, 16);
    const auto data = random_bytes(24 * 16, 9);
    fast.draw_pixels_at(0, 0, 24, 16, data.data(), bitness, false, 0, 0, 0);
    slow.draw_one_by_one(0, 0, 24, 16, data.data(), bitness, false, 0, 0, 0);
    EXPECT_EQ(fast.buffer(), slow.buffer()) << "bitness " << int(bitness);
    EXPECT_EQ(fast.bounds(), slow.bounds()) << "bitness " << int(bitness);
  }
}

}  // namespace esphome::epaper_spi::testing
