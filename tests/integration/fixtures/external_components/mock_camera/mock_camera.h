#pragma once

#include "esphome/components/camera/camera.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <vector>

namespace esphome::mock_camera {

/** Deterministic in-memory camera image.
 *  Byte i of frame N is (N + i) & 0xFF so tests can validate
 *  reassembled data from just the first byte.
 */
class MockCameraImage : public camera::CameraImage {
 public:
  MockCameraImage(size_t size, uint8_t frame_counter, uint8_t requesters) : requesters_(requesters) {
    this->data_.resize(size);
    for (size_t i = 0; i < size; i++) {
      this->data_[i] = static_cast<uint8_t>(frame_counter + i);
    }
  }
  uint8_t *get_data_buffer() override { return this->data_.data(); }
  size_t get_data_length() override { return this->data_.size(); }
  bool was_requested_by(camera::CameraRequester requester) const override {
    return (this->requesters_ & (1 << requester)) != 0;
  }

 protected:
  std::vector<uint8_t> data_;
  uint8_t requesters_;
};

class MockCameraImageReader : public camera::CameraImageReader {
 public:
  void set_image(std::shared_ptr<camera::CameraImage> image) override {
    this->image_ = std::move(image);
    this->offset_ = 0;
  }
  size_t available() const override { return this->image_ ? this->image_->get_data_length() - this->offset_ : 0; }
  uint8_t *peek_data_buffer() override { return this->image_->get_data_buffer() + this->offset_; }
  void consume_data(size_t consumed) override { this->offset_ += consumed; }
  void return_image() override {
    this->image_.reset();
    this->offset_ = 0;
  }

 protected:
  std::shared_ptr<camera::CameraImage> image_;
  size_t offset_{0};
};

/** Virtual camera producing deterministic frames on request or stream. */
class MockCamera : public camera::Camera {
 public:
  void setup() override {}
  void loop() override;
  void dump_config() override;

  void add_listener(camera::CameraListener *listener) override { this->listeners_.push_back(listener); }
  camera::CameraImageReader *create_image_reader() override { return new MockCameraImageReader(); }
  void request_image(camera::CameraRequester requester) override { this->single_requesters_ |= (1 << requester); }
  void start_stream(camera::CameraRequester requester) override { this->stream_requesters_ |= (1 << requester); }
  void stop_stream(camera::CameraRequester requester) override { this->stream_requesters_ &= ~(1 << requester); }

  void set_image_size(size_t size) { this->image_size_ = size; }
  void set_frame_interval(uint32_t ms) { this->frame_interval_ = ms; }

 protected:
  std::vector<camera::CameraListener *> listeners_;
  size_t image_size_{1024};
  uint32_t frame_interval_{50};
  uint32_t last_frame_ms_{0};
  uint8_t frame_counter_{0};
  uint8_t single_requesters_{0};
  uint8_t stream_requesters_{0};
};

}  // namespace esphome::mock_camera
