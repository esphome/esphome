#include "ssocr_reader.h"

#include "esphome/components/camera_decoder/jpeg_grayscale.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstring>

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

namespace esphome::ssocr_reader {

ESPHOME_LOG_TAG(TAG, "ssocr_reader");

void SSOCRReader::setup() {
  if (this->crop_ == nullptr) {
    this->mark_failed(LOG_STR("Could not allocate the crop buffer"));
    return;
  }
  this->camera_->add_listener(this);
  this->ocr_.reserve(this->crop_w_);

  // The decoder allocates the decoded frame, which needs PSRAM. Report a missing
  // PSRAM instead of failing later with an out-of-memory decode.
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
    ESP_LOGW(TAG, "No PSRAM detected; the decoder may not be able to allocate the decoded frame");
  }
}

void SSOCRReader::dump_config() {
  ESP_LOGCONFIG(TAG, "SSOCR Reader:");
  ESP_LOGCONFIG(TAG, "  Frame: %dx%d", this->img_width_, this->img_height_);
  ESP_LOGCONFIG(TAG, "  Crop: x=%d, y=%d, w=%d, h=%d", this->crop_x_, this->crop_y_, this->crop_w_, this->crop_h_);
  ESP_LOGCONFIG(TAG, "  Threshold: %u, Digits: %u, Debug: %s", this->ocr_.get_threshold_level(), this->digit_count_,
                this->debug_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  ESP_LOGCONFIG(TAG, "  Update Interval: %" PRIu32 " ms", this->get_update_interval());
  this->decoder_->dump_config();
}

void SSOCRReader::update() {
  if (this->debug_) {
    ESP_LOGD(TAG, "Update triggered (Interval cycle)");
  }

  // Request one frame. The state machine rejects frames that arrive while no
  // request is pending, so an idle camera frame cannot be processed twice.
  FrameState expected = FrameState::IDLE;
  if (this->frame_state_.compare_exchange_strong(expected, FrameState::REQUESTED)) {
    this->last_request_time_ = App.get_loop_component_start_time();
    this->camera_->request_image(camera::IDLE);
    ESP_LOGD(TAG, "Requesting frame");
  }
}

void SSOCRReader::on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  FrameState expected = FrameState::REQUESTED;
  if (this->frame_state_.compare_exchange_strong(expected, FrameState::AVAILABLE)) {
    this->pending_frame_ = image;
  }
}

void SSOCRReader::loop() {
  std::shared_ptr<esphome::camera::CameraImage> frame = std::move(this->pending_frame_);
  if (frame) {
    this->frame_state_.store(FrameState::PROCESSING);
    this->process_frame_(frame);
    frame.reset();
    this->frame_state_.store(FrameState::IDLE);
  }

  // Watchdog: a requested frame that never arrives must not block later polls.
  const uint32_t now = App.get_loop_component_start_time();
  if (this->frame_state_.load() == FrameState::REQUESTED && now - this->last_request_time_ > 5000) {
    ESP_LOGW(TAG, "Frame timeout");
    FrameState expected = FrameState::REQUESTED;
    if (this->frame_state_.compare_exchange_strong(expected, FrameState::IDLE)) {
      this->pending_frame_ = nullptr;
    }
  }
}

void SSOCRReader::process_frame_(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  if (image->get_data_length() == 0)
    return;

  camera::CameraImageSpec spec{};
  const camera::DecoderError error = this->decoder_->decode(image->get_data_buffer(), image->get_data_length(), &spec);
  if (error != camera::DECODER_ERROR_SUCCESS) {
    ESP_LOGW(TAG, "JPEG decode failed: %s", camera::to_string(error));
    return;
  }

  camera::Buffer *decoded = this->decoder_->get_output_buffer();
  if (decoded == nullptr || decoded->get_data_buffer() == nullptr) {
    ESP_LOGW(TAG, "Decoder returned no output buffer");
    return;
  }

  // A camera_window may shrink the frame; a larger one would break the configured crop.
  if (spec.width > this->img_width_ || spec.height > this->img_height_) {
    ESP_LOGW(TAG, "Decoded dimensions larger than config: %dx%d vs %dx%d", spec.width, spec.height, this->img_width_,
             this->img_height_);
    return;
  }

  if (!this->crop_frame_(decoded->get_data_buffer(), spec)) {
    ESP_LOGE(TAG, "Could not crop %dx%d at %d,%d from the %dx%d frame", this->crop_w_, this->crop_h_, this->crop_x_,
             this->crop_y_, spec.width, spec.height);
    return;
  }

  // Read one digit past the configured count so a crop that contains more digits is rejected
  // instead of publishing a truncated reading. The engine stops at the count it is given.
  const seven_segment_ocr::OcrResult result =
      this->ocr_.recognize(this->crop_, this->crop_w_, this->crop_h_, static_cast<uint8_t>(this->digit_count_ + 1));
  if (result.digits != this->digit_count_ || result.unknown != 0 || result.overflow) {
    ESP_LOGW(TAG, "Could not read %u digits (found %u, unknown %u)", this->digit_count_, result.digits, result.unknown);
    return;
  }

  ESP_LOGV(TAG, "Read value=%" PRIu32 " (confidence %.2f)", result.value, result.confidence);
  PUBLISH_STATE(this->value_sensor_, static_cast<float>(result.value));
  PUBLISH_STATE(this->confidence_sensor_, result.confidence * 100.0f);
}

bool SSOCRReader::crop_frame_(const uint8_t *src, const camera::CameraImageSpec &spec) {
  if (src == nullptr || this->crop_ == nullptr)
    return false;
  if (this->crop_x_ + this->crop_w_ > spec.width || this->crop_y_ + this->crop_h_ > spec.height)
    return false;

  const bool source_is_gray = spec.format == camera::PIXEL_FORMAT_GRAYSCALE;
  if (!source_is_gray && spec.format != camera::PIXEL_FORMAT_RGB888)
    return false;

  const size_t source_bpp = source_is_gray ? 1 : camera_decoder::RGB888_BYTES_PER_PIXEL;
  const size_t source_stride = source_bpp * spec.width;
  const uint8_t *source_row =
      src + static_cast<size_t>(this->crop_y_) * source_stride + static_cast<size_t>(this->crop_x_) * source_bpp;
  for (int y = 0; y < this->crop_h_; y++) {
    const uint8_t *row = source_row + static_cast<size_t>(y) * source_stride;
    uint8_t *dst = this->crop_ + static_cast<size_t>(y) * this->crop_w_;
    if (source_is_gray) {
      std::memcpy(dst, row, this->crop_w_);
      continue;
    }
    for (int x = 0; x < this->crop_w_; x++) {
      const uint8_t *pixel = row + static_cast<size_t>(x) * source_bpp;
      dst[x] = static_cast<uint8_t>((pixel[0] * camera_decoder::LUMA_RED + pixel[1] * camera_decoder::LUMA_GREEN +
                                     pixel[2] * camera_decoder::LUMA_BLUE) >>
                                    8);
    }
  }
  return true;
}

}  // namespace esphome::ssocr_reader
