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
  void draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorBitness bitness,
                            bool big_endian, int x_offset, int y_offset, int x_pad) override {
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
  Image image(data.data(), W, H, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE);
  RecordingDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.blits.size(), 1u);
  EXPECT_EQ(display.blits[0].bitness, display::COLOR_BITNESS_888);
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

// The base class fallback reads pixels in the image's byte order.
static void expect_fallback_decodes(bool big_endian) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE, big_endian);
  FallbackDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.pixels.size(), static_cast<size_t>(W * H));
  for (const auto &pixel : display.pixels) {
    const uint32_t rgb565 =
        display::ColorUtil::read_packed<display::COLOR_BITNESS_565>(data.data(), pixel.x + pixel.y * W, big_endian);
    const Color expected = display::ColorUtil::to_color(rgb565, display::COLOR_BITNESS_565);
    EXPECT_EQ(pixel.raw, expected.raw_32) << "at " << pixel.x << "," << pixel.y;
  }
}

TEST(ImageDraw, FallbackDecodesLittleEndian) { expect_fallback_decodes(false); }
TEST(ImageDraw, FallbackDecodesBigEndian) { expect_fallback_decodes(true); }

// The per pixel reader behind get_pixel() reads RGB565 in the image's byte order.
static void expect_reader_decodes(bool big_endian) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE, big_endian);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const uint32_t rgb565 =
          display::ColorUtil::read_packed<display::COLOR_BITNESS_565>(data.data(), x + y * W, big_endian);
      const Color expected(((rgb565 >> 11) << 3) | ((rgb565 >> 11) >> 2),
                           (((rgb565 >> 5) & 0x3F) << 2) | (((rgb565 >> 5) & 0x3F) >> 4),
                           ((rgb565 & 0x1F) << 3) | ((rgb565 & 0x1F) >> 2), 0xFF);
      EXPECT_EQ(image.get_pixel(x, y).raw_32, expected.raw_32) << "at " << x << "," << y;
    }
  }
}

TEST(ImageDraw, ReaderDecodesLittleEndian) { expect_reader_decodes(false); }
TEST(ImageDraw, ReaderDecodesBigEndian) { expect_reader_decodes(true); }

// A chroma keyed RGB565 draw stays per pixel and must match get_pixel().
static void expect_keyed_draw_matches_reader(bool big_endian) {
  const auto data = make_data(2);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB565, TRANSPARENCY_CHROMA_KEY, big_endian);
  FallbackDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_GT(display.pixels.size(), 0u);
  for (const auto &pixel : display.pixels) {
    EXPECT_EQ(pixel.raw, image.get_pixel(pixel.x, pixel.y).raw_32) << "at " << pixel.x << "," << pixel.y;
  }
}

TEST(ImageDraw, KeyedDrawMatchesReaderLittleEndian) { expect_keyed_draw_matches_reader(false); }
TEST(ImageDraw, KeyedDrawMatchesReaderBigEndian) { expect_keyed_draw_matches_reader(true); }

// The fallback keeps red and blue in the order the image stores them.
TEST(ImageDraw, FallbackKeepsRgbChannelOrder) {
  const auto data = make_data(3);
  Image image(data.data(), W, H, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE);
  FallbackDisplay display;
  image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
  ASSERT_EQ(display.pixels.size(), static_cast<size_t>(W * H));
  for (const auto &pixel : display.pixels) {
    // get_pixel() reports an opaque pixel with alpha set; the fallback decodes colour only
    const Color expected = image.get_pixel(pixel.x, pixel.y);
    EXPECT_EQ(pixel.raw, Color(expected.r, expected.g, expected.b).raw_32) << "at " << pixel.x << "," << pixel.y;
  }
}

// Each pixel the per pixel draw emits is the colour get_pixel() reads, and it skips exactly the pixels get_pixel()
// reports as transparent. Grayscale with alpha is left out: draw() blends it, get_pixel() returns the alpha.
TEST(ImageDraw, PerPixelDrawMatchesReader) {
  const auto data = make_data(4);
  for (ImageType type :
       {IMAGE_TYPE_BINARY, IMAGE_TYPE_GRAYSCALE, IMAGE_TYPE_RGB, IMAGE_TYPE_RGB565, IMAGE_TYPE_RGB111}) {
    for (Transparency transparency : {TRANSPARENCY_OPAQUE, TRANSPARENCY_CHROMA_KEY, TRANSPARENCY_ALPHA_CHANNEL}) {
      const bool bulk = transparency == TRANSPARENCY_OPAQUE && (type == IMAGE_TYPE_RGB || type == IMAGE_TYPE_RGB565);
      if (bulk || (type == IMAGE_TYPE_GRAYSCALE && transparency == TRANSPARENCY_ALPHA_CHANNEL))
        continue;
      for (bool big_endian : {false, true}) {
        Image image(data.data(), W, H, type, transparency, big_endian);
        FallbackDisplay display;
        image.draw(0, 0, &display, Color::WHITE, Color::BLACK);
        std::vector<int> drawn(W * H, 0);
        for (const auto &pixel : display.pixels) {
          ASSERT_TRUE(pixel.x >= 0 && pixel.x < W && pixel.y >= 0 && pixel.y < H);
          drawn[pixel.y * W + pixel.x]++;
          EXPECT_EQ(pixel.raw, image.get_pixel(pixel.x, pixel.y, Color::WHITE, Color::BLACK).raw_32)
              << "type " << type << " transparency " << transparency << " at " << pixel.x << "," << pixel.y;
        }
        for (int y = 0; y < H; y++) {
          for (int x = 0; x < W; x++) {
            const Color expected = image.get_pixel(x, y, Color::WHITE, Color::BLACK);
            const bool visible = type == IMAGE_TYPE_BINARY
                                     ? (expected == Color::WHITE || transparency == TRANSPARENCY_OPAQUE)
                                     : expected.w >= 0x80;
            EXPECT_EQ(drawn[y * W + x], visible ? 1 : 0)
                << "type " << type << " transparency " << transparency << " at " << x << "," << y;
          }
        }
      }
    }
  }
}

}  // namespace esphome::image::testing
