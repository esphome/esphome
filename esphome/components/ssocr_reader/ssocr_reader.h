#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SSOCR_READER

#include "esphome/components/camera/buffer.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/camera/decoder.h"
#include "esphome/components/seven_segment_ocr/seven_segment_ocr.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <cstdint>
#include <memory>

namespace esphome::ssocr_reader {

/// Reads a seven-segment display out of esp32_camera frames. The camera delivers
/// JPEG; a shared camera::Decoder decodes each frame once, the configured
/// rectangle is cropped out of that buffer as grayscale, and seven_segment_ocr
/// reads the digits. Frames are requested with the camera's idle requester, so
/// connected API clients receive the frames the reader asks for.
class SSOCRReader final : public PollingComponent, public camera::CameraListener {
 public:
  /// The camera, decoder, frame size and crop buffer never change, so they are constructor parameters.
  SSOCRReader(camera::Camera *camera, camera::Decoder *decoder, uint16_t frame_width, uint16_t frame_height,
              uint8_t *crop)
      : camera_(camera), decoder_(decoder), crop_(crop), img_width_(frame_width), img_height_(frame_height) {}

  void setup() override;
  void update() override;
  void loop() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  void dump_config() override;

  void set_threshold_level(uint8_t level) { this->ocr_.set_threshold_level(level); }
  void set_crop(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    this->crop_x_ = x;
    this->crop_y_ = y;
    this->crop_w_ = width;
    this->crop_h_ = height;
  }
  void set_digit_count(uint8_t count) { this->digit_count_ = count; }

  SUB_SENSOR(value)
  SUB_SENSOR(confidence)

 protected:
  static constexpr uint32_t FRAME_TIMEOUT_ID = 0;
  static constexpr uint32_t FRAME_TIMEOUT_MS = 5000;

  void process_frame_(const std::shared_ptr<camera::CameraImage> &image);
  /// Copies the configured rectangle out of a decoded frame as one byte per pixel.
  bool crop_frame_(camera::Buffer &decoded, const camera::CameraImageSpec &spec);

  camera::Camera *camera_{nullptr};
  camera::Decoder *decoder_{nullptr};
  uint8_t *crop_{nullptr};
  std::shared_ptr<camera::CameraImage> pending_frame_{nullptr};
  seven_segment_ocr::SevenSegmentOCR ocr_;
  uint16_t img_width_{0};
  uint16_t img_height_{0};
  uint16_t crop_x_{0};
  uint16_t crop_y_{0};
  uint16_t crop_w_{0};
  uint16_t crop_h_{0};
  uint8_t digit_count_{6};
};

}  // namespace esphome::ssocr_reader

#endif  // USE_SSOCR_READER
