#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "esphome/components/runtime_image/image_decoder.h"
#include "esphome/components/runtime_image/jpeg_decoder.h"
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

// 3x3 QOI, exercising all possible chunk types
static const uint8_t QOI_RGBA[] = {
    0x71, 0x6F, 0x69, 0x66,  // Header: 'qoif'
    0x00, 0x00, 0x00, 0x03,  // Width: 3
    0x00, 0x00, 0x00, 0x03,  // Height: 3
    0x04,                    // Channels: 4 (RGBA)
    0x00,                    // Colorspace: 0 (SRGB)
    0xC1,                    // 1. QOI_OP_RUN
    0x79,                    // 2. QOI_OP_DIFF
    0xAA, 0x79,              // 3. QOI_OP_LUMA
    0xFE, 0xC8, 0x64, 0x32,  // 4. QOI_OP_RGB
    0xFF, 0x78, 0x50, 0x28,
    0x64,                                           // 5. QOI_OP_RGBA
    0x31,                                           // 6. QOI_OP_INDEX
    0xC1,                                           // 7. QOI_OP_RUN
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01  // End Marker
};

static const uint8_t QOI_EXPECTED_RGBA[3][3][4] = {
    {{0x00, 0x00, 0x00, 0xFF}, {0x00, 0x00, 0x00, 0xFF}, {0x01, 0x00, 0xFF, 0xFF}},
    {{0x0A, 0x0A, 0x0A, 0xFF}, {0xC8, 0x64, 0x32, 0xFF}, {0x78, 0x50, 0x28, 0x64}},
    {{0x01, 0x00, 0xFF, 0xFF}, {0x01, 0x00, 0xFF, 0xFF}, {0x01, 0x00, 0xFF, 0xFF}}

};

/// Exposes the protected decoder machinery so reuse and eviction can be observed directly.
class TestableRuntimeImage : public RuntimeImage {
 public:
  explicit TestableRuntimeImage(ImageFormat format, image::Transparency transparency = image::TRANSPARENCY_OPAQUE)
      : RuntimeImage(format, image::IMAGE_TYPE_RGB, transparency, nullptr, false, 0, 0) {}

  TestableRuntimeImage(ImageFormat format, image::ImageType type, bool is_big_endian, int fixed_width, int fixed_height)
      : RuntimeImage(format, type, image::TRANSPARENCY_OPAQUE, nullptr, is_big_endian, fixed_width, fixed_height) {}

  ImageDecoder *decoder() { return this->decoder_.get(); }
};

/// Runs one full decode session. Returns true when every stage succeeded.
static bool decode_all(TestableRuntimeImage &img, const uint8_t *data, size_t len, ImageFormat format = AUTO) {
  std::vector<uint8_t> buffer(data, data + len);  // feed_data needs mutable bytes
  if (!img.begin_decode(len, format)) {
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

/// Feeds the image the way online_image's download loop does: append a small
/// chunk to a window, feed the window, drop what was consumed, repeat. A zero
/// return mid-stream means "need more data" and grows the window.
static bool decode_chunked(TestableRuntimeImage &img, const uint8_t *data, size_t len, size_t chunk_size) {
  if (!img.begin_decode(len)) {
    return false;
  }
  std::vector<uint8_t> window;
  size_t supplied = 0;
  while (supplied < len || !window.empty()) {
    if (supplied < len) {
      size_t take = std::min(chunk_size, len - supplied);
      window.insert(window.end(), data + supplied, data + supplied + take);
      supplied += take;
    }
    int consumed = img.feed_data(window.data(), window.size());
    if (consumed < 0 || (consumed == 0 && supplied >= len)) {
      return false;  // decode error, or stuck with all data supplied
    }
    window.erase(window.begin(), window.begin() + consumed);
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

template<size_t H, size_t W>
static void expect_pixels_rgba(TestableRuntimeImage &img, const uint8_t (&expected)[H][W][4]) {
  ASSERT_EQ(img.get_width(), static_cast<int>(W));
  ASSERT_EQ(img.get_height(), static_cast<int>(H));
  for (size_t y = 0; y < H; y++) {
    for (size_t x = 0; x < W; x++) {
      SCOPED_TRACE(::testing::Message() << "pixel (" << x << "," << y << ")");
      Color color = img.get_pixel(x, y);
      EXPECT_THAT((std::array<uint8_t, 4>{color.r, color.g, color.b, color.w}),
                  ::testing::ElementsAreArray(expected[y][x]));
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

TEST(RuntimeImageDecoder, ColorTableGrowsAndShrinksAcrossReuse) {
  TestableRuntimeImage img(BMP);

  // Small palette first: the retained table is allocated at 4 entries.
  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP)));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  ImageDecoder *first = img.decoder();

  // Growing to 8 entries on the reused decoder must reallocate, not overflow.
  ASSERT_TRUE(decode_all(img, BMP_8BPP_BIG, sizeof(BMP_8BPP_BIG)));
  expect_pixels(img, BMP_8BPP_BIG_EXPECTED);
  EXPECT_EQ(img.decoder(), first);

  // Shrinking back must not surface stale colors from the larger table.
  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP)));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first);
}

TEST(RuntimeImageDecoder, ChunkedFeedDecodesLikeDownloadLoop) {
  TestableRuntimeImage img(BMP);

  ASSERT_TRUE(decode_chunked(img, BMP_24BPP, sizeof(BMP_24BPP), 16));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  ImageDecoder *first = img.decoder();

  // Chunked again on the warm decoder: the cross-call resume state
  // (current_index_ / paint_index_) must have been fully reset.
  ASSERT_TRUE(decode_chunked(img, BMP_24BPP, sizeof(BMP_24BPP), 16));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first);
}

TEST(RuntimeImageDecoder, FormatSwitchEvictsMismatchedDecoder) {
  // Drive the format switch through begin_decode()'s format parameter, the way
  // a dynamic-format producer (online_image MIME detection) does.
  TestableRuntimeImage img(AUTO);

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP), BMP));
  ASSERT_NE(img.decoder(), nullptr);
  ASSERT_EQ(img.decoder()->get_format(), BMP);
  expect_pixels(img, BMP_24BPP_EXPECTED);

  // Same explicit format again: the decoder must stay warm.
  ImageDecoder *bmp_decoder = img.decoder();
  ASSERT_TRUE(decode_all(img, BMP_8BPP, sizeof(BMP_8BPP), BMP));
  expect_pixels(img, BMP_8BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), bmp_decoder);

  // Different format: the stale decoder must be evicted and recreated.
  ASSERT_TRUE(decode_all(img, PNG_RGB, sizeof(PNG_RGB), PNG));
  EXPECT_EQ(img.decoder()->get_format(), PNG);
  expect_pixels(img, PNG_RGB_EXPECTED);

  // And back again.
  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP), BMP));
  EXPECT_EQ(img.decoder()->get_format(), BMP);
  expect_pixels(img, BMP_24BPP_EXPECTED);
}

TEST(RuntimeImageDecoder, AutoFormatFallsBackToConfiguredAndKeepsDecoderWarm) {
  // With a configured format, an AUTO begin_decode() must resolve to the
  // configured format before the reuse check instead of evicting the decoder.
  TestableRuntimeImage img(BMP);

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP), AUTO));
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->get_format(), BMP);

  ASSERT_TRUE(decode_all(img, BMP_24BPP, sizeof(BMP_24BPP), AUTO));
  expect_pixels(img, BMP_24BPP_EXPECTED);
  EXPECT_EQ(img.decoder(), first) << "AUTO must not evict the configured-format decoder";
}

TEST(RuntimeImageDecoder, AutoWithoutConfiguredFormatFails) {
  // Neither a configured format nor an explicit one: there is nothing to decode with.
  TestableRuntimeImage img(AUTO);
  EXPECT_FALSE(img.begin_decode(64));
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

#ifdef USE_RUNTIME_IMAGE_JPEG
// 8x8 gradient JPEG (quality 90). JPEG is lossy, so the test asserts that a
// reused decoder reproduces the exact same pixels, not absolute colors.
static const uint8_t JPEG_GRADIENT[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x03, 0x03,
    0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0A, 0x07, 0x07, 0x06, 0x08, 0x0C, 0x0A, 0x0C, 0x0C, 0x0B, 0x0A,
    0x0B, 0x0B, 0x0D, 0x0E, 0x12, 0x10, 0x0D, 0x0E, 0x11, 0x0E, 0x0B, 0x0B, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15,
    0x15, 0x15, 0x0C, 0x0F, 0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x03,
    0x04, 0x04, 0x05, 0x04, 0x05, 0x09, 0x05, 0x05, 0x09, 0x14, 0x0D, 0x0B, 0x0D, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x08, 0x00, 0x08, 0x03, 0x01, 0x22, 0x00,
    0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
    0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62,
    0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
    0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85,
    0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00,
    0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04,
    0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31,
    0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09,
    0x23, 0x33, 0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A,
    0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
    0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
    0xFA, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0xE5, 0x3E, 0x0B, 0xFE,
    0xC8, 0x7F, 0xEA, 0x3F, 0xD0, 0xBD, 0x3F, 0x86, 0x8A, 0x28, 0xAA, 0xC2, 0x62, 0x6A, 0xFB, 0x25, 0xA9, 0xD5, 0xC0,
    0x7C, 0x6B, 0x9D, 0x7F, 0x62, 0xD3, 0xFD, 0xEF, 0xF5, 0xF7, 0x9F, 0xFF, 0xD9,
};

static std::vector<uint8_t> pixel_bytes(TestableRuntimeImage &img) {
  const uint8_t *start = img.get_data_start();
  return std::vector<uint8_t>(start, start + img.get_width_stride() * img.get_height());
}

// Shrinking a PNG keeps the first source pixel of each span, as the JPEG path does: the even rows and columns
TEST(RuntimeImageDecoder, ShrunkPngKeepsTheFirstPixelOfEachSpan) {
  TestableRuntimeImage img(PNG, image::IMAGE_TYPE_RGB, false, 2, 2);
  ASSERT_TRUE(decode_all(img, PNG_RGB, sizeof(PNG_RGB), PNG));
  ASSERT_EQ(img.get_width(), 2);
  ASSERT_EQ(img.get_height(), 2);
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 2; col++) {
      const Color pixel = img.get_pixel(col, row);
      const uint8_t *expected = PNG_RGB_EXPECTED[row * 2][col * 2];
      EXPECT_EQ(pixel.r, expected[0]) << col << "," << row;
      EXPECT_EQ(pixel.g, expected[1]) << col << "," << row;
      EXPECT_EQ(pixel.b, expected[2]) << col << "," << row;
    }
  }
}

TEST(RuntimeImageDecoder, JpegDecoderStaysWarmAcrossDecodes) {
  TestableRuntimeImage img(JPEG);

  ASSERT_TRUE(decode_all(img, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_EQ(img.get_width(), 8);
  ASSERT_EQ(img.get_height(), 8);
  std::vector<uint8_t> first_pixels = pixel_bytes(img);
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);

  ASSERT_TRUE(decode_all(img, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  EXPECT_EQ(img.decoder(), first);
  EXPECT_EQ(pixel_bytes(img), first_pixels) << "reused decoder must reproduce identical pixels";
}

// The RGB565 word at (x, y) of a decoded image, in its own byte order
static uint16_t rgb565_word(const TestableRuntimeImage &img, int x, int y) {
  const uint8_t *p = img.get_data_start() + (y * img.get_width() + x) * 2;
  return p[0] | (p[1] << 8);
}

// JPEGDEC's colour conversion differs by one step between its SIMD and scalar builds, so each
// channel is checked within one step; the byte order is still exact
static void expect_rgb565_near(uint16_t actual, uint16_t expected, const char *order, size_t pixel) {
  EXPECT_NEAR(actual >> 11, expected >> 11, 1) << order << " pixel " << pixel << " red";
  EXPECT_NEAR((actual >> 5) & 0x3F, (expected >> 5) & 0x3F, 1) << order << " pixel " << pixel << " green";
  EXPECT_NEAR(actual & 0x1F, expected & 0x1F, 1) << order << " pixel " << pixel << " blue";
}

TEST(RuntimeImageDecoder, JpegDecodesDirectlyToScaledRgb565WithBothByteOrders) {
  // Halving the 8x8 gradient makes JPEGDEC decode it at half size, which averages each 2x2 block of pixels
  TestableRuntimeImage full(JPEG, image::IMAGE_TYPE_RGB565, false, 8, 8);
  TestableRuntimeImage rgb565_little_endian(JPEG, image::IMAGE_TYPE_RGB565, false, 4, 4);
  TestableRuntimeImage rgb565_big_endian(JPEG, image::IMAGE_TYPE_RGB565, true, 4, 4);

  ASSERT_TRUE(decode_all(full, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_TRUE(decode_all(rgb565_little_endian, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_TRUE(decode_all(rgb565_big_endian, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_EQ(rgb565_little_endian.get_width(), 4);
  ASSERT_EQ(rgb565_little_endian.get_height(), 4);
  ASSERT_EQ(rgb565_big_endian.get_width(), 4);
  ASSERT_EQ(rgb565_big_endian.get_height(), 4);

  const uint8_t *little = rgb565_little_endian.get_data_start();
  const uint8_t *big = rgb565_big_endian.get_data_start();
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      int red = 0;
      int green = 0;
      int blue = 0;
      for (int dy = 0; dy < 2; dy++) {
        for (int dx = 0; dx < 2; dx++) {
          const uint16_t word = rgb565_word(full, col * 2 + dx, row * 2 + dy);
          red += word >> 11;
          green += (word >> 5) & 0x3F;
          blue += word & 0x1F;
        }
      }
      const uint16_t expected = ((red + 2) / 4) << 11 | ((green + 2) / 4) << 5 | (blue + 2) / 4;
      const size_t i = row * 4 + col;
      expect_rgb565_near(little[i * 2] | (little[i * 2 + 1] << 8), expected, "little-endian", i);
      expect_rgb565_near((big[i * 2] << 8) | big[i * 2 + 1], expected, "big-endian", i);
    }
  }
}

// The generic draw() path gets JPEGDEC's half size RGB8888 output. It must match the half size RGB565 decode checked
// above. A full size RGB8888 decode is no reference: JPEGDEC's x86 SSE code writes it with red and blue swapped.
TEST(RuntimeImageDecoder, JpegDecodesToScaledRgb) {
  TestableRuntimeImage rgb565(JPEG, image::IMAGE_TYPE_RGB565, false, 4, 4);
  TestableRuntimeImage rgb(JPEG, image::IMAGE_TYPE_RGB, false, 4, 4);
  ASSERT_TRUE(decode_all(rgb565, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_TRUE(decode_all(rgb, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  ASSERT_EQ(static_cast<JpegDecoder *>(rgb.decoder())->get_scale(), 2);

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      const uint16_t word = rgb565_word(rgb565, col, row);
      const Color pixel = rgb.get_pixel(col, row);
      // RGB565 keeps the top 5 or 6 bits of each channel
      EXPECT_LE(std::abs(pixel.r - ((word >> 11) << 3)), 8) << "pixel " << col << "," << row << " red";
      EXPECT_LE(std::abs(pixel.g - (((word >> 5) & 0x3F) << 2)), 4) << "pixel " << col << "," << row << " green";
      EXPECT_LE(std::abs(pixel.b - ((word & 0x1F) << 3)), 8) << "pixel " << col << "," << row << " blue";
    }
  }
}

// Target pixel index each source pixel paints when `source` pixels map onto `target`, the way
// prepare_rgb565_scaling_ rounds: source n covers targets ceil(n * target / source) up to the next start
static uint16_t scaled_boundary(int n, int source, int target) { return (n * target + source - 1) / source; }

// Every target pixel of a PNG drawn at a size that is not a whole multiple: enlarged with spans 1 or 2 pixels wide, and
// shrunk with spans of 0, 1 or 2 pixels
TEST(RuntimeImageDecoder, ScaledPngPaintsEachSourcePixelOverItsSpan) {
  // A fixed size keeps the aspect ratio, so both are square
  for (auto [width, height] : {std::pair{6, 6}, std::pair{3, 3}}) {
    TestableRuntimeImage img(PNG, image::IMAGE_TYPE_RGB, false, width, height);
    ASSERT_TRUE(decode_all(img, PNG_RGB, sizeof(PNG_RGB), PNG));
    ASSERT_EQ(img.get_width(), width);
    ASSERT_EQ(img.get_height(), height);
    int painted = 0;
    for (int m = 0; m < 4; m++) {
      for (int n = 0; n < 4; n++) {
        const uint8_t *expected = PNG_RGB_EXPECTED[m][n];
        for (int y = scaled_boundary(m, 4, height); y < scaled_boundary(m + 1, 4, height); y++) {
          for (int x = scaled_boundary(n, 4, width); x < scaled_boundary(n + 1, 4, width); x++) {
            const Color pixel = img.get_pixel(x, y);
            EXPECT_EQ(pixel.r, expected[0]) << width << "x" << height << " at " << x << "," << y;
            EXPECT_EQ(pixel.g, expected[1]) << width << "x" << height << " at " << x << "," << y;
            EXPECT_EQ(pixel.b, expected[2]) << width << "x" << height << " at " << x << "," << y;
            painted++;
          }
        }
      }
    }
    EXPECT_EQ(painted, width * height);
  }
}

// JPEGDEC decodes at the smallest of full, 1/2, 1/4 and 1/8 size that still covers the buffer
TEST(RuntimeImageDecoder, JpegDecodesAtTheSmallestSizeThatCoversTheBuffer) {
  for (auto [size, scale] :
       {std::pair{8, 1}, std::pair{5, 1}, std::pair{4, 2}, std::pair{3, 2}, std::pair{2, 4}, std::pair{1, 8}}) {
    TestableRuntimeImage img(JPEG, image::IMAGE_TYPE_RGB565, false, size, size);
    ASSERT_TRUE(decode_all(img, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
    EXPECT_EQ(img.get_width(), size);
    EXPECT_EQ(static_cast<JpegDecoder *>(img.decoder())->get_scale(), scale) << size << "x" << size;
  }
}

// Every scaled direct decode must equal the unscaled one sampled through the boundary table,
// for shrinking, growing and a ratio that is not a whole number
TEST(RuntimeImageDecoder, JpegDirectRgb565ScalesBySourcePixelSpans) {
  TestableRuntimeImage full(JPEG, image::IMAGE_TYPE_RGB565, false, 8, 8);
  ASSERT_TRUE(decode_all(full, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));

  // Sizes JPEGDEC cannot reach by scaling, so the full size decode is spread over the buffer
  for (int size : {5, 6, 16}) {
    TestableRuntimeImage scaled(JPEG, image::IMAGE_TYPE_RGB565, false, size, size);
    ASSERT_TRUE(decode_all(scaled, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
    ASSERT_EQ(scaled.get_width(), size);
    ASSERT_EQ(scaled.get_height(), size);
    for (int n = 0; n < 8; n++) {
      for (int m = 0; m < 8; m++) {
        for (int y = scaled_boundary(m, 8, size); y < scaled_boundary(m + 1, 8, size); y++) {
          for (int x = scaled_boundary(n, 8, size); x < scaled_boundary(n + 1, 8, size); x++) {
            EXPECT_EQ(rgb565_word(scaled, x, y), rgb565_word(full, n, m))
                << size << "x" << size << " pixel " << x << "," << y;
          }
        }
      }
    }
  }
}

// A second decode into the same image reuses the boundary tables and gives the same pixels
TEST(RuntimeImageDecoder, JpegDirectRgb565RepeatsWithReusedTables) {
  TestableRuntimeImage img(JPEG, image::IMAGE_TYPE_RGB565, false, 6, 6);
  ASSERT_TRUE(decode_all(img, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  std::vector<uint8_t> first_pixels = pixel_bytes(img);
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);

  ASSERT_TRUE(decode_all(img, JPEG_GRADIENT, sizeof(JPEG_GRADIENT)));
  EXPECT_EQ(img.decoder(), first);
  EXPECT_EQ(pixel_bytes(img), first_pixels);
}
#endif  // USE_RUNTIME_IMAGE_JPEG

TEST(RuntimeImageDecoder, QoiDecoderStaysWarmAcrossDecodes) {
  TestableRuntimeImage img(QOI, image::TRANSPARENCY_ALPHA_CHANNEL);

  ASSERT_TRUE(decode_all(img, QOI_RGBA, sizeof(QOI_RGBA)));
  expect_pixels_rgba(img, QOI_EXPECTED_RGBA);
  ImageDecoder *first = img.decoder();
  ASSERT_NE(first, nullptr);

  ASSERT_TRUE(decode_all(img, QOI_RGBA, sizeof(QOI_RGBA)));
  expect_pixels_rgba(img, QOI_EXPECTED_RGBA);
  EXPECT_EQ(img.decoder(), first) << "decoder must be reused, not reallocated";
}

TEST(RuntimeImageDecoder, QoiChunkedFeedDecodesLikeDownloadLoop) {
  TestableRuntimeImage img(QOI, image::TRANSPARENCY_ALPHA_CHANNEL);

  ASSERT_TRUE(decode_chunked(img, QOI_RGBA, sizeof(QOI_RGBA), 10));
  expect_pixels_rgba(img, QOI_EXPECTED_RGBA);
  ImageDecoder *first = img.decoder();

  // Chunked again on the warm decoder: the cross-call resume state
  // (current_index_ / paint_index_) must have been fully reset.
  ASSERT_TRUE(decode_chunked(img, QOI_RGBA, sizeof(QOI_RGBA), 10));
  expect_pixels_rgba(img, QOI_EXPECTED_RGBA);
  EXPECT_EQ(img.decoder(), first);
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
