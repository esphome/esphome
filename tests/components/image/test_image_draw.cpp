#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/display/display.h"
#include "esphome/components/image/image.h"

namespace esphome::image::testing {

class TestDisplay : public display::Display {
 public:
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 48; }
};

struct Blit {
  int x, y, w, h;
  const uint8_t *ptr;
  display::ColorBitness bitness;
  bool big_endian;
  int x_offset, y_offset, x_pad;
};

// Records the bulk calls and counts the per pixel calls Image::draw() makes.
class RecordingDisplay : public TestDisplay {
 public:
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    this->blits.push_back({x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad});
  }
  void draw_pixel_at(int x, int y, Color color) override { this->pixels++; }

  std::vector<Blit> blits;
  int pixels{0};
};

// Lets the bulk call fall through to the base class and keeps the pixels it produces.
class FallbackDisplay : public TestDisplay {
 public:
  void draw_pixel_at(int x, int y, Color color) override { this->pixels.push_back({x, y, color.raw_32}); }

  struct Pixel {
    int x, y;
    uint32_t raw;
  };
  std::vector<Pixel> pixels;
};

static constexpr int W = 8, H = 6;

static std::vector<uint8_t> make_data(int bytes_per_pixel) {
  std::vector<uint8_t> data(W * H * bytes_per_pixel);
  uint8_t value = 1;
  for (auto &byte : data)
    byte = value++;
  return data;
}

// Draws an opaque RGB565 image at (x, y) and returns the single bulk call it made.
static Blit draw_one(int x, int y, const std::vector<uint8_t> &data, RecordingDisplay &display,
                     bool big_endian = false) {
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE, big_endian);
  image.draw(x, y, &display, Color::WHITE, Color::BLACK);
  EXPECT_EQ(display.blits.size(), 1u);
  EXPECT_EQ(display.pixels, 0);
  return display.blits.at(0);
}

static void expect_window(const Blit &blit, int x, int y, int w, int h, int x_offset, int y_offset, int x_pad) {
  EXPECT_EQ(blit.x, x);
  EXPECT_EQ(blit.y, y);
  EXPECT_EQ(blit.w, w);
  EXPECT_EQ(blit.h, h);
  EXPECT_EQ(blit.x_offset, x_offset);
  EXPECT_EQ(blit.y_offset, y_offset);
  EXPECT_EQ(blit.x_pad, x_pad);
}

TEST(ImageDraw, OpaqueRgb565IsOneBulkCall) {
  const auto data = make_data(2);
  RecordingDisplay display;
  const Blit blit = draw_one(10, 20, data, display);
  expect_window(blit, 10, 20, W, H, 0, 0, 0);
  EXPECT_EQ(blit.ptr, data.data());
  EXPECT_EQ(blit.bitness, display::COLOR_BITNESS_565);
  EXPECT_FALSE(blit.big_endian);
}

TEST(ImageDraw, BigEndianRgb565IsPassedThrough) {
  const auto data = make_data(2);
  RecordingDisplay display;
  EXPECT_TRUE(draw_one(0, 0, data, display, true).big_endian);
}

TEST(ImageDraw, OpaqueRgbIsOneBulkCall) {
  const auto data = make_data(3);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE, true);  // byte order only applies to 565
  RecordingDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  EXPECT_EQ(display.blits[0].bitness, display::COLOR_BITNESS_888);
  EXPECT_FALSE(display.blits[0].big_endian);
}

TEST(ImageDraw, ClippingShrinksTheWindow) {
  const auto data = make_data(2);
  RecordingDisplay display;
  display.start_clipping(12, 22, 15, 24);  // right and bottom are exclusive
  expect_window(draw_one(10, 20, data, display), 12, 22, 3, 2, 2, 2, W - 5);
}

TEST(ImageDraw, PartlyOffTheTopLeftIsClamped) {
  const auto data = make_data(2);
  RecordingDisplay display;
  expect_window(draw_one(-3, -2, data, display), 0, 0, W - 3, H - 2, 3, 2, 0);
}

TEST(ImageDraw, PartlyOffTheBottomRightIsClamped) {
  const auto data = make_data(2);
  RecordingDisplay display;
  expect_window(draw_one(60, 44, data, display), 60, 44, 4, 4, 0, 0, W - 4);
}

TEST(ImageDraw, FullyOffScreenDrawsNothing) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  image.draw(64, 0, &display, Color::WHITE, Color::BLACK);
  image.draw(0, -H, &display, Color::WHITE, Color::BLACK);
  EXPECT_TRUE(display.blits.empty());
  EXPECT_EQ(display.pixels, 0);
}

TEST(ImageDraw, TransparentAndOtherFormatsStayPerPixel) {
  const auto rgb565 = make_data(2);
  const auto gray = make_data(1);
  Image keyed(rgb565.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_CHROMA_KEY);
  Image grayscale(gray.data(), W, H, IMAGE_TYPE_GRAYSCALE, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  keyed.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  grayscale.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  EXPECT_TRUE(display.blits.empty());
  EXPECT_GT(display.pixels, 0);
}

// The base class fallback must read the pixels in the byte order the image declares.
static void expect_fallback_decodes(bool big_endian) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE, big_endian);
  FallbackDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.pixels.size(), static_cast<size_t>(W * H));
  for (const auto &pixel : display.pixels) {
    const uint32_t rgb565 =
        display::ColorUtil::read_packed<display::COLOR_BITNESS_565>(data.data(), pixel.x + pixel.y * W, big_endian);
    const Color expected = display::ColorUtil::to_color(rgb565, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565);
    EXPECT_EQ(pixel.raw, expected.raw_32) << "at " << pixel.x << "," << pixel.y;
  }
}

TEST(ImageDraw, FallbackDecodesLittleEndian) { expect_fallback_decodes(false); }
TEST(ImageDraw, FallbackDecodesBigEndian) { expect_fallback_decodes(true); }

}  // namespace esphome::image::testing
