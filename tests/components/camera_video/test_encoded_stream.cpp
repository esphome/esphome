#include "esphome/components/camera_video/encoded_stream.h"
#include "esphome/components/camera_video/h264_stream.h"

#include <type_traits>

#include "gtest/gtest.h"

namespace esphome::camera_video {
namespace {

TEST(EncodedVideoStream, H264FramesUseGenericFrameContract) {
  EXPECT_TRUE((std::is_base_of_v<EncodedVideoFrame, H264Frame>) );
}

TEST(EncodedVideoFormat, ValidatesRationalFrameRate) {
  EncodedVideoFormat format{VideoCodec::H264, 1920, 1080, 30000, 1001};
  EXPECT_TRUE(format.is_valid());
  format.fps_den = 0;
  EXPECT_FALSE(format.is_valid());
  format.fps_den = 1;
  format.width = 0;
  EXPECT_FALSE(format.is_valid());
}

TEST(EncodedVideoFormat, DistinguishesCodecs) {
  EncodedVideoFormat jpeg{VideoCodec::JPEG, 640, 480, 30, 1};
  EncodedVideoFormat h264{VideoCodec::H264, 640, 480, 30, 1};
  EXPECT_TRUE(jpeg.is_valid());
  EXPECT_TRUE(h264.is_valid());
  EXPECT_NE(jpeg.codec, h264.codec);
}

}  // namespace
}  // namespace esphome::camera_video
