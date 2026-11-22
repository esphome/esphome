#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SSOCR_READER

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
/// reads the digits.
class SSOCRReader final : public PollingComponent, public esphome::camera::CameraListener {
 public:
  /// The camera, decoder, frame size and crop buffer never change, so they are constructor parameters.
  SSOCRReader(esphome::camera::Camera *camera, esphome::camera::Decoder *decoder, int frame_width, int frame_height,
              uint8_t *crop)
      : camera_(camera), decoder_(decoder), img_width_(frame_width), img_height_(frame_height), crop_(crop) {}

  void setup() override;
  void update() override;
  void loop() override;
  void on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) override;
  void dump_config() override;

  void set_threshold_level(uint8_t level) { this->ocr_.set_threshold_level(level); }
  void set_crop(int x, int y, int width, int height) {
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

  void process_frame_(const std::shared_ptr<esphome::camera::CameraImage> &image);
  /// Copies the configured rectangle out of a decoded frame as one byte per pixel.
  bool crop_frame_(const uint8_t *src, const camera::CameraImageSpec &spec);

  esphome::camera::Camera *camera_{nullptr};
  esphome::camera::Decoder *decoder_{nullptr};
  int img_width_{0};
  int img_height_{0};
  uint8_t *crop_{nullptr};
  seven_segment_ocr::SevenSegmentOCR ocr_;
  int crop_x_{0};
  int crop_y_{0};
  int crop_w_{0};
  int crop_h_{0};
  uint8_t digit_count_{6};
  bool frame_requested_{false};
  std::shared_ptr<esphome::camera::CameraImage> pending_frame_{nullptr};
};

}  // namespace esphome::ssocr_reader

#endif  // USE_SSOCR_READER
