#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/camera_decoder/jpeg_grayscale.h"

namespace esphome::camera_decoder::testing {

namespace {

/// Appends one RGB888 pixel to the buffer.
void add_pixel_(std::vector<uint8_t> &buffer, uint8_t red, uint8_t green, uint8_t blue) {
  buffer.push_back(red);
  buffer.push_back(green);
  buffer.push_back(blue);
}

}  // namespace

TEST(JpegGrayscale, WhiteStaysWhite) {
  std::vector<uint8_t> buffer;
  add_pixel_(buffer, 255, 255, 255);

  rgb888_to_grayscale_in_place(buffer.data(), 1);

  EXPECT_EQ(buffer[0], 255);
}

TEST(JpegGrayscale, BlackStaysBlack) {
  std::vector<uint8_t> buffer;
  add_pixel_(buffer, 0, 0, 0);

  rgb888_to_grayscale_in_place(buffer.data(), 1);

  EXPECT_EQ(buffer[0], 0);
}

TEST(JpegGrayscale, ChannelsUseTheBt601Weights) {
  std::vector<uint8_t> buffer;
  add_pixel_(buffer, 255, 0, 0);
  add_pixel_(buffer, 0, 255, 0);
  add_pixel_(buffer, 0, 0, 255);

  rgb888_to_grayscale_in_place(buffer.data(), 3);

  EXPECT_EQ(buffer[0], 76);   // 255 * 77 >> 8
  EXPECT_EQ(buffer[1], 149);  // 255 * 150 >> 8
  EXPECT_EQ(buffer[2], 28);   // 255 * 29 >> 8
}

TEST(JpegGrayscale, CompactionDoesNotReadOverwrittenPixels) {
  // Three distinct pixels: the forward pass writes bytes 0..2 while the sources
  // of the later pixels live at 3..8, so every pixel must keep its own value.
  std::vector<uint8_t> buffer;
  add_pixel_(buffer, 255, 0, 0);
  add_pixel_(buffer, 0, 255, 0);
  add_pixel_(buffer, 0, 0, 255);

  rgb888_to_grayscale_in_place(buffer.data(), 3);

  EXPECT_EQ(buffer[0], 76);
  EXPECT_EQ(buffer[1], 149);
  EXPECT_EQ(buffer[2], 28);
  // Nothing is written past the compacted pixels.
  EXPECT_EQ(buffer[3], 0);
  EXPECT_EQ(buffer[4], 255);
  EXPECT_EQ(buffer[5], 0);
  EXPECT_EQ(buffer[6], 0);
  EXPECT_EQ(buffer[7], 0);
  EXPECT_EQ(buffer[8], 255);
}

TEST(JpegGrayscale, ZeroPixelsWritesNothing) {
  std::vector<uint8_t> buffer;
  add_pixel_(buffer, 10, 20, 30);

  rgb888_to_grayscale_in_place(buffer.data(), 0);

  EXPECT_EQ(buffer[0], 10);
  EXPECT_EQ(buffer[1], 20);
  EXPECT_EQ(buffer[2], 30);
}

}  // namespace esphome::camera_decoder::testing
