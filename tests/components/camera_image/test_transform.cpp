#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/camera_image/transform.h"

namespace esphome::camera_image::testing {

namespace {

// A 2 wide, 3 tall frame:
//   1 2
//   3 4
//   5 6
constexpr int FRAME_WIDTH = 2;
constexpr int FRAME_HEIGHT = 3;
constexpr std::array<uint8_t, 6> FRAME = {1, 2, 3, 4, 5, 6};

}  // namespace

TEST(CameraImageTransform, Rotate90) {
  std::array<uint8_t, 6> out{};
  ASSERT_TRUE(rotate_crop(1, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_HEIGHT, FRAME_WIDTH, 2, 3, 90));
  EXPECT_EQ(out, (std::array<uint8_t, 6>{5, 3, 1, 6, 4, 2}));
}

TEST(CameraImageTransform, Rotate180) {
  std::array<uint8_t, 6> out{};
  ASSERT_TRUE(
      rotate_crop(1, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT, 2, 2, 180));
  EXPECT_EQ(out, (std::array<uint8_t, 6>{6, 5, 4, 3, 2, 1}));
}

TEST(CameraImageTransform, Rotate270) {
  std::array<uint8_t, 6> out{};
  ASSERT_TRUE(
      rotate_crop(1, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_HEIGHT, FRAME_WIDTH, 2, 3, 270));
  EXPECT_EQ(out, (std::array<uint8_t, 6>{2, 4, 6, 1, 3, 5}));
}

TEST(CameraImageTransform, CropInsideALargerFrameUsesTheSourceStride) {
  // The same 2x3 picture sits at column 1 of a 4 wide frame; 9 is outside the crop.
  constexpr std::array<uint8_t, 12> source = {9, 1, 2, 9, 9, 3, 4, 9, 9, 5, 6, 9};
  std::array<uint8_t, 6> out{};
  ASSERT_TRUE(
      rotate_crop(1, out.data(), source.data() + 1, FRAME_WIDTH, FRAME_HEIGHT, FRAME_HEIGHT, FRAME_WIDTH, 4, 3, 90));
  EXPECT_EQ(out, (std::array<uint8_t, 6>{5, 3, 1, 6, 4, 2}));
}

TEST(CameraImageTransform, TwoBytePixelsMoveTogether) {
  // Each pixel is a pair of bytes (n, n + 10); a rotation must not split them.
  constexpr std::array<uint8_t, 12> source = {1, 11, 2, 12, 3, 13, 4, 14, 5, 15, 6, 16};
  std::array<uint8_t, 12> out{};
  ASSERT_TRUE(
      rotate_crop(2, out.data(), source.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_HEIGHT, FRAME_WIDTH, 4, 6, 90));
  EXPECT_EQ(out, (std::array<uint8_t, 12>{5, 15, 3, 13, 1, 11, 6, 16, 4, 14, 2, 12}));
}

TEST(CameraImageTransform, ThreeBytePixelsMoveTogether) {
  constexpr std::array<uint8_t, 18> source = {1, 11, 21, 2, 12, 22, 3, 13, 23, 4, 14, 24, 5, 15, 25, 6, 16, 26};
  std::array<uint8_t, 18> out{};
  ASSERT_TRUE(
      rotate_crop(3, out.data(), source.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT, 6, 6, 180));
  EXPECT_EQ(out, (std::array<uint8_t, 18>{6, 16, 26, 5, 15, 25, 4, 14, 24, 3, 13, 23, 2, 12, 22, 1, 11, 21}));
}

TEST(CameraImageTransform, UnsupportedPixelSizeAndRotationAreRejected) {
  std::array<uint8_t, 6> out{};
  out.fill(7);
  EXPECT_FALSE(
      rotate_crop(4, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_HEIGHT, FRAME_WIDTH, 2, 3, 90));
  EXPECT_FALSE(rotate_crop(1, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT, 2, 2, 0));
  EXPECT_FALSE(
      rotate_crop(1, out.data(), FRAME.data(), FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT, 2, 2, 45));
  // Nothing is written when the request is rejected.
  EXPECT_EQ(out, (std::array<uint8_t, 6>{7, 7, 7, 7, 7, 7}));
}

}  // namespace esphome::camera_image::testing
