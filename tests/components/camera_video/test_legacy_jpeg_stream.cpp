#include "esphome/components/camera_video/legacy_jpeg_stream.h"

#include "gtest/gtest.h"

#include <array>
#include <memory>

namespace esphome::camera_video {
namespace {

class TestJpegImage final : public camera::CameraImage {
 public:
  uint8_t *get_data_buffer() override { return this->data_.data(); }
  size_t get_data_length() override { return this->data_.size(); }
  bool was_requested_by(camera::CameraRequester requester) const override { return requester == camera::WEB_REQUESTER; }

 private:
  std::array<uint8_t, 4> data_{{0xFF, 0xD8, 0xFF, 0xD9}};
};

class TestJpegCamera final : public camera::Camera {
 public:
  void add_listener(camera::CameraListener *listener) override { this->listener_ = listener; }
  camera::CameraImageReader *create_image_reader() override { return nullptr; }
  void request_image(camera::CameraRequester requester) override { (void) requester; }
  void start_stream(camera::CameraRequester requester) override {
    if (requester == camera::WEB_REQUESTER)
      this->starts_++;
  }
  void stop_stream(camera::CameraRequester requester) override {
    if (requester == camera::WEB_REQUESTER)
      this->stops_++;
  }
  void emit(const std::shared_ptr<camera::CameraImage> &image) {
    if (this->listener_ != nullptr)
      this->listener_->on_camera_image(image);
  }
  int starts_{0};
  int stops_{0};

 private:
  camera::CameraListener *listener_{nullptr};
};

class TestJpegListener final : public EncodedVideoStreamListener {
 public:
  void on_encoded_frame(const FrameRef<EncodedVideoFrame> &frame) override {
    this->last_frame_ = frame;
    this->received_++;
  }
  FrameRef<EncodedVideoFrame> last_frame_;
  int received_{0};
};

TEST(LegacyJpegStream, SharesImageAndStopsOnlyForFinalSubscriber) {
  TestJpegCamera camera;
  LegacyJpegStreamAdapter stream(&camera, 640, 480, 15);
  EncodedVideoFormat format;
  ASSERT_TRUE(stream.get_format(&format));
  EXPECT_EQ(format.codec, VideoCodec::JPEG);
  EXPECT_EQ(format.width, 640);
  size_t codec_length = 123;
  EXPECT_TRUE(stream.get_codec_config(nullptr, 0, &codec_length));
  EXPECT_EQ(codec_length, 0);

  TestJpegListener first;
  TestJpegListener second;
  ASSERT_TRUE(stream.start_stream(&first));
  ASSERT_TRUE(stream.start_stream(&first));
  ASSERT_TRUE(stream.start_stream(&second));
  EXPECT_EQ(camera.starts_, 1);

  auto image = std::make_shared<TestJpegImage>();
  std::weak_ptr<camera::CameraImage> weak_image = image;
  camera.emit(image);
  EXPECT_EQ(first.received_, 1);
  EXPECT_EQ(second.received_, 1);
  ASSERT_TRUE(first.last_frame_);
  EXPECT_EQ(first.last_frame_->get_data_length(), 4);
  EXPECT_EQ(first.last_frame_.get(), second.last_frame_.get());
  EXPECT_TRUE(first.last_frame_->is_keyframe());
  image.reset();
  EXPECT_FALSE(weak_image.expired());
  first.last_frame_.reset();
  EXPECT_FALSE(weak_image.expired());
  second.last_frame_.reset();
  EXPECT_TRUE(weak_image.expired());

  stream.stop_stream(&first);
  EXPECT_EQ(camera.stops_, 0);
  stream.stop_stream(&second);
  EXPECT_EQ(camera.stops_, 1);
}

}  // namespace
}  // namespace esphome::camera_video
