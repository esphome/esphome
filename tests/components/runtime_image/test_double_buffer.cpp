#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

// 3x2 24bpp BMP (rows padded to 4 bytes)
static const uint8_t BMP_3X2[] = {
    0x42, 0x4D, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x22, 0x11, 0x77, 0x88, 0x99, 0xEF, 0xCD, 0xAB, 0x00,
    0x00, 0x00, 0x20, 0x10, 0xE0, 0x40, 0xC0, 0x30, 0xA0, 0x60, 0x50, 0x00, 0x00, 0x00,
};

// 2x1 24bpp BMP (row padded to 8 bytes)
static const uint8_t BMP_2X1[] = {
    0x42, 0x4D, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x06, 0x05, 0x04, 0x00, 0x00,
};

template<size_t N> static void load(RuntimeImage &img, const uint8_t (&bmp)[N]) {
  std::vector<uint8_t> data(bmp, bmp + N);
  ASSERT_TRUE(img.begin_decode(data.size()));
  ASSERT_EQ(img.feed_data(data.data(), data.size()), static_cast<int>(data.size()));
  ASSERT_TRUE(img.end_decode());
}

class DoubleBufferedImage : public RuntimeImage {
 public:
  DoubleBufferedImage() : RuntimeImage(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE) {
    this->set_double_buffer(true);
  }
};

// The image on show is not decoded over: the new one appears only when it is complete
TEST(RuntimeImageDoubleBuffer, ImageOnShowStaysUntilTheNextIsDecoded) {
  DoubleBufferedImage img;
  load(img, BMP_3X2);
  const uint8_t *first = img.get_data_start();
  ASSERT_NE(first, nullptr);
  const std::vector<uint8_t> shown(first, first + 3 * 2 * 3);

  std::vector<uint8_t> data(BMP_2X1, BMP_2X1 + sizeof(BMP_2X1));
  ASSERT_TRUE(img.begin_decode(data.size()));
  ASSERT_EQ(img.feed_data(data.data(), data.size()), static_cast<int>(data.size()));
  EXPECT_EQ(img.get_data_start(), first);
  EXPECT_EQ(img.get_width(), 3);
  EXPECT_EQ(img.get_height(), 2);
  EXPECT_EQ(std::vector<uint8_t>(first, first + shown.size()), shown);

  ASSERT_TRUE(img.end_decode());
  EXPECT_NE(img.get_data_start(), first);
  EXPECT_EQ(img.get_width(), 2);
  EXPECT_EQ(img.get_height(), 1);
}

// Images of one size take turns in the same two buffers
TEST(RuntimeImageDoubleBuffer, SameSizeImagesReuseBothBuffers) {
  DoubleBufferedImage img;
  load(img, BMP_3X2);
  const uint8_t *first = img.get_data_start();
  load(img, BMP_3X2);
  const uint8_t *second = img.get_data_start();
  EXPECT_NE(second, first);
  load(img, BMP_3X2);
  EXPECT_EQ(img.get_data_start(), first);
  load(img, BMP_3X2);
  EXPECT_EQ(img.get_data_start(), second);
}

TEST(RuntimeImageDoubleBuffer, AbortedDecodeLeavesTheImageOnShow) {
  DoubleBufferedImage img;
  load(img, BMP_3X2);
  const uint8_t *first = img.get_data_start();

  std::vector<uint8_t> data(BMP_2X1, BMP_2X1 + sizeof(BMP_2X1));
  ASSERT_TRUE(img.begin_decode(data.size()));
  img.feed_data(data.data(), data.size() / 2 + 8);
  img.abort_decode();
  EXPECT_FALSE(img.is_decoding());
  EXPECT_EQ(img.get_data_start(), first);
  EXPECT_EQ(img.get_width(), 3);
  EXPECT_TRUE(img.is_loaded());

  load(img, BMP_2X1);
  EXPECT_EQ(img.get_width(), 2);
}

// Without double buffering an aborted decode has drawn over the image, so it goes
TEST(RuntimeImageDoubleBuffer, AbortedDecodeWithOneBufferReleasesTheImage) {
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  load(img, BMP_3X2);
  std::vector<uint8_t> data(BMP_3X2, BMP_3X2 + sizeof(BMP_3X2));
  ASSERT_TRUE(img.begin_decode(data.size()));
  img.feed_data(data.data(), data.size() / 2 + 8);
  img.abort_decode();
  EXPECT_EQ(img.get_data_start(), nullptr);
  EXPECT_FALSE(img.is_loaded());
}

// Both buffers are freed; LeakSanitizer reports one that is not
TEST(RuntimeImageDoubleBuffer, ReleaseFreesBothBuffers) {
  DoubleBufferedImage img;
  load(img, BMP_3X2);
  load(img, BMP_3X2);
  img.release();
  EXPECT_EQ(img.get_data_start(), nullptr);
  EXPECT_EQ(img.get_width(), 0);
  EXPECT_FALSE(img.is_loaded());
}

TEST(RuntimeImageDoubleBuffer, ExternalBufferIsRefused) {
  DoubleBufferedImage img;
  std::vector<uint8_t> buffer(img.get_buffer_size(3, 2));
  EXPECT_FALSE(img.set_external_buffer(buffer.data(), 3, 2));
  load(img, BMP_3X2);
  EXPECT_NE(img.get_data_start(), buffer.data());
}

}  // namespace esphome::runtime_image::testing
