#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>

#include "esphome/components/runtime_image/image_decoder.h"
#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

// 3x2 24bpp BMP, every pixel a unique color (rows padded to 4 bytes)
static const uint8_t BMP_24BPP[] = {
    0x42, 0x4D, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x22, 0x11, 0x77, 0x88, 0x99, 0xEF, 0xCD, 0xAB, 0x00,
    0x00, 0x00, 0x20, 0x10, 0xE0, 0x40, 0xC0, 0x30, 0xA0, 0x60, 0x50, 0x00, 0x00, 0x00,
};

static const uint8_t BMP_24BPP_EXPECTED[2][3][3] = {
    {{0xE0, 0x10, 0x20}, {0x30, 0xC0, 0x40}, {0x50, 0x60, 0xA0}},
    {{0x11, 0x22, 0x33}, {0x99, 0x88, 0x77}, {0xAB, 0xCD, 0xEF}},
};

// 3x2 8bpp BMP with a 4-entry color table
static const uint8_t BMP_8BPP[] = {
    0x42, 0x4D, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x20, 0x10, 0x00, 0xD0, 0xE0, 0xF0, 0x00, 0x00, 0xFF,
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x03, 0x02, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00,
};

static const uint8_t BMP_8BPP_EXPECTED[2][3][3] = {
    {{0x10, 0x20, 0x30}, {0xF0, 0xE0, 0xD0}, {0x00, 0xFF, 0x00}},
    {{0xFF, 0x00, 0xFF}, {0x00, 0xFF, 0x00}, {0xF0, 0xE0, 0xD0}},
};

// 3x2 8bpp BMP with an 8-entry color table, all colors distinct from BMP_8BPP's
static const uint8_t BMP_8BPP_BIG[] = {
    0x42, 0x4D, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x18, 0x08,
    0x00, 0xA8, 0xB8, 0xC8, 0x00, 0xFF, 0x80, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00, 0x80, 0xFF, 0x00, 0x55, 0x99,
    0x11, 0x00, 0xCC, 0x00, 0x66, 0x00, 0x44, 0x22, 0xEE, 0x00, 0x01, 0x06, 0x04, 0x00, 0x07, 0x05, 0x03, 0x00,
};

static const uint8_t BMP_8BPP_BIG_EXPECTED[2][3][3] = {
    {{0xEE, 0x22, 0x44}, {0x11, 0x99, 0x55}, {0x80, 0xFF, 0x00}},
    {{0xC8, 0xB8, 0xA8}, {0x66, 0x00, 0xCC}, {0xFF, 0x80, 0x00}},
};

// 4x4 RGB PNG, every pixel a unique color
static const uint8_t PNG_RGB[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x00, 0x00, 0x00, 0x26, 0x93, 0x09, 0x29, 0x00, 0x00, 0x00, 0x38, 0x49,
    0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x60, 0x64, 0x62, 0x16, 0x50, 0x30, 0x58, 0xB0, 0xE1, 0xC0, 0xFF, 0xFF, 0x0C,
    0x0C, 0x0E, 0x0C, 0x50, 0xEC, 0xE0, 0xE0, 0xC0, 0x50, 0xCF, 0xF0, 0x9F, 0xA1, 0xFE, 0xFF, 0xFF, 0x7A, 0x86, 0xFA,
    0xFF, 0x0C, 0x0C, 0x42, 0x26, 0x61, 0xA9, 0xCE, 0x8A, 0xFF, 0xEE, 0xEC, 0x5A, 0x7D, 0xF6, 0x3D, 0x00, 0x81, 0xCB,
    0x12, 0x4D, 0xB3, 0xFB, 0xD4, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const uint8_t PNG_RGB_EXPECTED[4][4][3] = {
    {{0x01, 0x02, 0x03}, {0x10, 0x20, 0x30}, {0xA0, 0xB0, 0xC0}, {0xFF, 0xFF, 0x00}},
    {{0x40, 0x00, 0x00}, {0x00, 0x40, 0x00}, {0x00, 0x00, 0x40}, {0x40, 0x40, 0x40}},
    {{0x7F, 0x00, 0xFF}, {0x00, 0x7F, 0xFF}, {0xFF, 0x7F, 0x00}, {0x7F, 0xFF, 0x00}},
    {{0x12, 0x34, 0x56}, {0x65, 0x43, 0x21}, {0xFE, 0xDC, 0xBA}, {0xAB, 0xCD, 0xEF}},
};

/// Exposes the protected decoder machinery so reuse and eviction can be observed directly.
class TestableRuntimeImage : public RuntimeImage {
 public:
  explicit TestableRuntimeImage(ImageFormat format)
      : RuntimeImage(format, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE, nullptr, false, 0, 0) {}

  ImageDecoder *decoder() { return this->decoder_.get(); }

  /// Simulates the state a dynamic-format producer (PR #16337) would leave behind:
  /// a cached decoder whose format no longer matches the image's format.
  /// TODO: once #16337 adds a public way to change the format, drive the mismatch
  /// through it and delete this seam.
  void plant_decoder(ImageFormat format) { this->decoder_ = this->create_decoder_(format); }
};

/// Runs one full decode session. Returns true when every stage succeeded.
static bool decode_all(TestableRuntimeImage &img, const uint8_t *data, size_t len) {
  std::vector<uint8_t> buffer(data, data + len);  // feed_data needs mutable bytes
  if (!img.begin_decode(len)) {
    return false;
  }
  size_t offset = 0;
  while (offset < len) {
    int consumed = img.feed_data(buffer.data() + offset, len - offset);
    if (consumed <= 0) {
      return false;  // decode error, or no progress despite full data
    }
    offset += consumed;
  }
  return img.end_decode();
}

template<size_t H, size_t W> static void expect_pixels(TestableRuntimeImage &img, const uint8_t (&expected)[H][W][3]) {
  ASSERT_EQ(img.get_width(), static_cast<int>(W));
  ASSERT_EQ(img.get_height(), static_cast<int>(H));
  for (size_t y = 0; y < H; y++) {
    for (size_t x = 0; x < W; x++) {
      SCOPED_TRACE(::testing::Message() << "pixel (" << x << "," << y << ")");
      Color color = img.get_pixel(x, y);
      EXPECT_THAT((std::array<uint8_t, 3>{color.r, color.g, color.b}), ::testing::ElementsAreArray(expected[y][x]));
    }
  }
}

TEST(RuntimeImageDecoder, DecoderStaysWarmAcrossDecodes) {
  TestableRuntimeImage img(BMP);

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP)));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP)));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first) << "decoder must be reused, not reallocated";
}

TEST(RuntimeImageDecoder, SecondDecodeStartsClean) {
  TestableRuntimeImage img(BMP);

  // Palettized decode, then a 24bpp decode, then palettized again, all on the
  // same decoder: each session must produce correct pixels for its own image.
  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP)));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  ImageDecoder *first = img.decoder();

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP)));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first);

  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP)));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first);
}

TEST(RuntimeImageDecoder, ShrunkenColorTableHasNoStaleColors) {
  TestableRuntimeImage img(BMP);

  // An 8-entry palette fills the retained color table; the following 4-entry
  // image must only ever produce its own colors, never leftovers from the
  // larger table beyond its entry count.
  ASSERT_TRUE(decode_all(img, BMP_8BPP_BIG, sizeof(BMP_8BPP_BIG)));
  expect_pixels(img, BMP_8BPP_BIG_EXPECTED);
  ImageDecoder *first = img.decoder();

  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP)));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first);
}

TEST(RuntimeImageDecoder, FormatSwitchEvictsMismatchedDecoder) {
  // PNG image holding a stale BMP decoder: begin_decode must evict and recreate.
  TestableRuntimeImage png_img(PNG);
  png_img.plant_decoder(BMP);
  ASSERT_NE(png_img.decoder(), nullptr);
  ASSERT_EQ(png_img.decoder()->get_format(), BMP);

  ASSERT_TRUE(decode_all(png_img, PNG_RGB, sizeof(PNG_RGB)));
  EXPECT_EQ(png_img.decoder()->get_format(), PNG);
  expect_pixels(png_img, PNG_RGB_EXPECTED);

  // And the other direction: BMP image holding a stale PNG decoder.
  TestableRuntimeImage bmp_img(BMP);
  bmp_img.plant_decoder(PNG);
  ASSERT_NE(bmp_img.decoder(), nullptr);
  ASSERT_EQ(bmp_img.decoder()->get_format(), PNG);

  ASSERT_TRUE(decode_all(bmp_img, BMP_24BPP, sizeof(BMP_24BPP)));
  EXPECT_EQ(bmp_img.decoder()->get_format(), BMP);
  expect_pixels(bmp_img, BMP_24BPP_EXPECTED);
}

TEST(RuntimeImageDecoder, ReleaseKeepsDecoderWarm) {
  TestableRuntimeImage img(PNG);

  ASSERT_TRUE(decode_all(img, PNG_RGB, sizeof(PNG_RGB)));
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);

  img.release();
  EXPECT_EQ(img.decoder(), first) << "release() must keep the decoder for reuse";
  EXPECT_FALSE(img.is_decoding());
  EXPECT_EQ(img.get_width(), 0);
  EXPECT_EQ(img.get_height(), 0);

  ASSERT_TRUE(decode_all(img, PNG_RGB, sizeof(PNG_RGB)));
  expect_pixels(img, PNG_RGB_EXPECTED);
  EXPECT_EQ(img.decoder(), first);
}

TEST(RuntimeImageDecoder, FailedDecodeRecovers) {
  TestableRuntimeImage img(BMP);

  uint8_t garbage[32];
  memset(garbage, 'X', sizeof(garbage));
  ASSERT_TRUE(img.begin_decode(sizeof(garbage)));
  EXPECT_LT(img.feed_data(garbage, sizeof(garbage)), 0) << "garbage must fail to decode";
  img.release();

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP)));
  expect_pixels(img, BMP_24BPP_EXPECTED);
}

TEST(RuntimeImageDecoder, SessionFlagsTrackLifecycle) {
  TestableRuntimeImage img(BMP);
  std::vector<uint8_t> buffer(BMP_24BPP, BMP_24BPP + sizeof(BMP_24BPP));

  ASSERT_TRUE(img.begin_decode(buffer.size()));
  EXPECT_TRUE(img.is_decoding());
  EXPECT_FALSE(img.is_decode_finished());

  ASSERT_EQ(img.feed_data(buffer.data(), buffer.size()), static_cast<int>(buffer.size()));
  EXPECT_TRUE(img.is_decode_finished()) << "all pixel data consumed";

  ASSERT_TRUE(img.end_decode());
  EXPECT_FALSE(img.is_decoding()) << "end_decode() must close the session";
  EXPECT_FALSE(img.is_decode_finished()) << "no session means nothing is 'finished'";
}

}  // namespace esphome::runtime_image::testing
