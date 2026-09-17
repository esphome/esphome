#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/display/display.h"
#include "esphome/components/image/image.h"

namespace esphome::image::testing {

struct Blit {
  int x, y, w, h;
  const uint8_t *ptr;
  display::ColorBitness bitness;
  bool big_endian;
  int x_offset, y_offset, x_pad;
};

// Records the bulk calls and the per pixel calls Image::draw() makes.
class RecordingDisplay : public display::Display {
 public:
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    this->blits.push_back({x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad});
  }
  void draw_pixel_at(int x, int y, Color color) override { this->pixels++; }
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 48; }

  std::vector<Blit> blits;
  int pixels{0};
};

// Same display, but the bulk call falls through to the base class so the
// pixels it produces can be checked.
class FallbackDisplay : public display::Display {
 public:
  void draw_pixel_at(int x, int y, Color color) override { this->pixels.push_back({x, y, color.raw_32}); }
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 48; }

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

TEST(ImageDraw, OpaqueRgb565IsOneBulkCall) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  image.draw(10, 20, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  EXPECT_EQ(display.pixels, 0);
  const auto &blit = display.blits[0];
  EXPECT_EQ(blit.x, 10);
  EXPECT_EQ(blit.y, 20);
  EXPECT_EQ(blit.w, W);
  EXPECT_EQ(blit.h, H);
  EXPECT_EQ(blit.ptr, data.data());
  EXPECT_EQ(blit.bitness, display::COLOR_BITNESS_565);
  EXPECT_FALSE(blit.big_endian);
  EXPECT_EQ(blit.x_offset, 0);
  EXPECT_EQ(blit.y_offset, 0);
  EXPECT_EQ(blit.x_pad, 0);
}

TEST(ImageDraw, OpaqueRgbIsOneBulkCall) {
  const auto data = make_data(3);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE);
  image.set_big_endian(true);  // only meaningful for 16 bit pixels
  RecordingDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  EXPECT_EQ(display.blits[0].bitness, display::COLOR_BITNESS_888);
  EXPECT_FALSE(display.blits[0].big_endian);
}

TEST(ImageDraw, BigEndianRgb565IsPassedThrough) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  image.set_big_endian(true);
  RecordingDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  EXPECT_TRUE(display.blits[0].big_endian);
}

TEST(ImageDraw, ClippingShrinksTheWindow) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  display.start_clipping(12, 22, 15, 24);  // right and bottom are exclusive
  image.draw(10, 20, &display, Color::WHITE, Color::BLACK);
  display.end_clipping();
  ASSERT_EQ(display.blits.size(), 1u);
  const auto &blit = display.blits[0];
  EXPECT_EQ(blit.x, 12);
  EXPECT_EQ(blit.y, 22);
  EXPECT_EQ(blit.w, 3);
  EXPECT_EQ(blit.h, 2);
  EXPECT_EQ(blit.x_offset, 2);
  EXPECT_EQ(blit.y_offset, 2);
  EXPECT_EQ(blit.x_pad, W - 5);
}

TEST(ImageDraw, PartlyOffTheTopLeftIsClamped) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  image.draw(-3, -2, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  const auto &blit = display.blits[0];
  EXPECT_EQ(blit.x, 0);
  EXPECT_EQ(blit.y, 0);
  EXPECT_EQ(blit.w, W - 3);
  EXPECT_EQ(blit.h, H - 2);
  EXPECT_EQ(blit.x_offset, 3);
  EXPECT_EQ(blit.y_offset, 2);
  EXPECT_EQ(blit.x_pad, 0);
}

TEST(ImageDraw, PartlyOffTheBottomRightIsClamped) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  image.draw(60, 44, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  const auto &blit = display.blits[0];
  EXPECT_EQ(blit.x, 60);
  EXPECT_EQ(blit.y, 44);
  EXPECT_EQ(blit.w, 4);
  EXPECT_EQ(blit.h, 4);
  EXPECT_EQ(blit.x_offset, 0);
  EXPECT_EQ(blit.y_offset, 0);
  EXPECT_EQ(blit.x_pad, W - 4);
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

// The base class fallback must decode the pixels the same way the per pixel
// readers do, in both byte orders.
static void expect_fallback_decodes(bool big_endian) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  image.set_big_endian(big_endian);
  FallbackDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.pixels.size(), static_cast<size_t>(W * H));
  for (const auto &pixel : display.pixels) {
    const uint8_t *pos = data.data() + (pixel.x + pixel.y * W) * 2;
    const uint16_t rgb565 = big_endian ? (pos[0] << 8) | pos[1] : (pos[1] << 8) | pos[0];
    const Color expected = display::ColorUtil::to_color(rgb565, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565);
    EXPECT_EQ(pixel.raw, expected.raw_32) << "at " << pixel.x << "," << pixel.y;
  }
}

TEST(ImageDraw, FallbackDecodesLittleEndian) { expect_fallback_decodes(false); }
TEST(ImageDraw, FallbackDecodesBigEndian) { expect_fallback_decodes(true); }

}  // namespace esphome::image::testing
