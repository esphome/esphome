#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "esphome/components/camera_video/h264_annexb.h"

namespace esphome::camera_video {
namespace {

std::vector<uint8_t> nal_types(const std::vector<uint8_t> &buffer) {
  const uint8_t *cursor = buffer.data();
  const uint8_t *end = cursor + buffer.size();
  H264AnnexBNal nal;
  std::vector<uint8_t> result;
  while (next_annexb_nal(&cursor, end, &nal)) {
    result.push_back(nal.type());
  }
  EXPECT_EQ(cursor, end);
  return result;
}

TEST(CameraVideoAnnexB, ParsesSpsAndPps) {
  EXPECT_EQ(nal_types({0, 0, 0, 1, 0x67, 0x42, 0, 0, 1, 0x68, 0xce}), (std::vector<uint8_t>{7, 8}));
}

TEST(CameraVideoAnnexB, SkipsEmptyNalsAndTrailingZeros) {
  EXPECT_EQ(nal_types({0, 0, 1, 0, 0, 1, 0x65, 0x80, 0, 0, 0}), (std::vector<uint8_t>{5}));
  EXPECT_TRUE(nal_types({0, 0, 0, 1, 0, 0, 1, 0, 0}).empty());
}

TEST(CameraVideoAnnexB, HandlesInvalidAndMultipleNals) {
  EXPECT_TRUE(nal_types({1, 2, 3, 4}).empty());
  EXPECT_EQ(nal_types({0, 0, 1, 0x61, 0xff, 0, 0, 1, 0x41, 0xaa}), (std::vector<uint8_t>{1, 1}));
}

}  // namespace
}  // namespace esphome::camera_video
