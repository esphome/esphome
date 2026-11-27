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

namespace testing {
class SSOCRReaderTestAccess;
}  // namespace testing

/// Reads a seven-segment display out of esp32_camera frames. The camera delivers
/// JPEG; a shared camera::Decoder decodes each frame once, the configured
/// rectangle is cropped out of that buffer as grayscale, and seven_segment_ocr
/// reads the digits. Frames are requested with the camera's component requester,
/// so API and web clients ignore them.
class SSOCRReader final : public PollingComponent, public camera::CameraListener {
 public:
  /// Everything but the threshold and the digit count is required and never changes.
  SSOCRReader(camera::Camera *camera, camera::Decoder *decoder, uint16_t frame_width, uint16_t frame_height,
              uint8_t *crop, uint16_t crop_x, uint16_t crop_y, uint16_t crop_width, uint16_t crop_height)
      : camera_(camera),
        decoder_(decoder),
        crop_(crop),
        img_width_(frame_width),
        img_height_(frame_height),
        crop_x_(crop_x),
        crop_y_(crop_y),
        crop_w_(crop_width),
        crop_h_(crop_height) {}

  void setup() override;
  void update() override;
  void loop() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  void dump_config() override;

  void set_threshold_level(uint8_t level) { this->ocr_.set_threshold_level(level); }
  void set_digit_count(uint8_t count) { this->digit_count_ = count; }

  SUB_SENSOR(value)
  SUB_SENSOR(confidence)

 protected:
  friend class testing::SSOCRReaderTestAccess;

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
