#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "esphome/components/ssocr_reader/ssocr_reader.h"

namespace esphome::ssocr_reader::testing {

class SSOCRReaderTestAccess {
 public:
  static bool crop_frame(SSOCRReader &reader, camera::Buffer &decoded, const camera::CameraImageSpec &spec) {
    return reader.crop_frame_(decoded, spec);
  }
};

namespace {

constexpr uint16_t FRAME_WIDTH = 6;
constexpr uint16_t FRAME_HEIGHT = 5;
constexpr uint16_t CROP_WIDTH = 3;
constexpr uint16_t CROP_HEIGHT = 2;
constexpr uint8_t UNTOUCHED = 0xAA;

class VectorBuffer : public camera::Buffer {
 public:
  explicit VectorBuffer(std::vector<uint8_t> data) : data_(std::move(data)) {}
  uint8_t *get_data_buffer() override { return this->data_.data(); }
  size_t get_data_length() override { return this->data_.size(); }

 protected:
  std::vector<uint8_t> data_;
};

/// A grayscale frame whose pixel values equal their index.
std::vector<uint8_t> gray_frame() {
  std::vector<uint8_t> frame(FRAME_WIDTH * FRAME_HEIGHT);
  for (size_t i = 0; i < frame.size(); i++)
    frame[i] = static_cast<uint8_t>(i);
  return frame;
}

/// Crops one frame into a buffer prefilled with UNTOUCHED.
struct Crop {
  Crop(uint16_t x, uint16_t y)
      : reader(nullptr, nullptr, FRAME_WIDTH, FRAME_HEIGHT, pixels.data(), x, y, CROP_WIDTH, CROP_HEIGHT) {}

  bool run(std::vector<uint8_t> data, camera::PixelFormat format) {
    VectorBuffer decoded(std::move(data));
    const camera::CameraImageSpec spec{FRAME_WIDTH, FRAME_HEIGHT, format};
    return SSOCRReaderTestAccess::crop_frame(this->reader, decoded, spec);
  }

  std::vector<uint8_t> pixels = std::vector<uint8_t>(CROP_WIDTH * CROP_HEIGHT, UNTOUCHED);
  SSOCRReader reader;
};

const std::vector<uint8_t> UNTOUCHED_CROP(CROP_WIDTH *CROP_HEIGHT, UNTOUCHED);

}  // namespace

TEST(SSOCRReaderCrop, CopiesTheGrayscaleRectangle) {
  Crop crop(2, 1);

  ASSERT_TRUE(crop.run(gray_frame(), camera::PIXEL_FORMAT_GRAYSCALE));

  EXPECT_EQ(crop.pixels, (std::vector<uint8_t>{8, 9, 10, 14, 15, 16}));
}

TEST(SSOCRReaderCrop, AcceptsARectangleOnTheFarEdges) {
  Crop crop(FRAME_WIDTH - CROP_WIDTH, FRAME_HEIGHT - CROP_HEIGHT);

  ASSERT_TRUE(crop.run(gray_frame(), camera::PIXEL_FORMAT_GRAYSCALE));

  EXPECT_EQ(crop.pixels, (std::vector<uint8_t>{21, 22, 23, 27, 28, 29}));
}

TEST(SSOCRReaderCrop, ConvertsRgb888ToLuma) {
  std::vector<uint8_t> frame(FRAME_WIDTH * FRAME_HEIGHT * 3, 0);
  const auto set_pixel = [&frame](int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
    const size_t offset = (y * FRAME_WIDTH + x) * 3;
    frame[offset] = red;
    frame[offset + 1] = green;
    frame[offset + 2] = blue;
  };
  set_pixel(1, 1, 255, 255, 255);
  set_pixel(2, 1, 255, 0, 0);
  set_pixel(3, 1, 0, 255, 0);
  set_pixel(1, 2, 0, 0, 255);
  set_pixel(3, 2, 255, 255, 255);
  Crop crop(1, 1);

  ASSERT_TRUE(crop.run(std::move(frame), camera::PIXEL_FORMAT_RGB888));

  EXPECT_EQ(crop.pixels, (std::vector<uint8_t>{255, 76, 149, 28, 0, 255}));
}

TEST(SSOCRReaderCrop, RejectsARectangleOutsideTheFrame) {
  Crop too_far_right(FRAME_WIDTH - CROP_WIDTH + 1, 0);
  Crop too_far_down(0, FRAME_HEIGHT - CROP_HEIGHT + 1);

  EXPECT_FALSE(too_far_right.run(gray_frame(), camera::PIXEL_FORMAT_GRAYSCALE));
  EXPECT_FALSE(too_far_down.run(gray_frame(), camera::PIXEL_FORMAT_GRAYSCALE));

  EXPECT_EQ(too_far_right.pixels, UNTOUCHED_CROP);
  EXPECT_EQ(too_far_down.pixels, UNTOUCHED_CROP);
}

TEST(SSOCRReaderCrop, RejectsABufferShorterThanTheFrame) {
  Crop gray(0, 0);
  Crop rgb(0, 0);

  EXPECT_FALSE(gray.run(std::vector<uint8_t>(FRAME_WIDTH * FRAME_HEIGHT - 1), camera::PIXEL_FORMAT_GRAYSCALE));
  EXPECT_FALSE(rgb.run(std::vector<uint8_t>(FRAME_WIDTH * FRAME_HEIGHT * 3 - 1), camera::PIXEL_FORMAT_RGB888));

  EXPECT_EQ(gray.pixels, UNTOUCHED_CROP);
  EXPECT_EQ(rgb.pixels, UNTOUCHED_CROP);
}

TEST(SSOCRReaderCrop, RejectsAnUnsupportedPixelFormat) {
  Crop crop(0, 0);

  EXPECT_FALSE(crop.run(std::vector<uint8_t>(FRAME_WIDTH * FRAME_HEIGHT * 2), camera::PIXEL_FORMAT_RGB565));

  EXPECT_EQ(crop.pixels, UNTOUCHED_CROP);
}

}  // namespace esphome::ssocr_reader::testing
