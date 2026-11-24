#include "tflite_camera.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::tflite_camera {

static const char *const TAG = "tflite_camera";

void TFLiteCamera::setup() {
  if (this->camera_ == nullptr || this->decoder_ == nullptr) {
    this->mark_failed(LOG_STR("Camera or decoder missing"));
    return;
  }
  this->camera_->add_listener(this);
}

void TFLiteCamera::dump_config() {
  ESP_LOGCONFIG(TAG, "TFLite Camera:\n  Minimum interval: %" PRIu32 " ms", this->min_interval_ms_);
  if (this->decoder_ != nullptr)
    this->decoder_->dump_config();
}

void TFLiteCamera::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (image == nullptr || this->decoder_ == nullptr || this->runtime_ == nullptr)
    return;
  const uint32_t now = millis();
  if (this->has_run_ && this->min_interval_ms_ > 0 && now - this->last_run_ms_ < this->min_interval_ms_)
    return;

  camera::CameraImageSpec spec{};
  const camera::DecoderError error = this->decoder_->decode(image->get_data_buffer(), image->get_data_length(), &spec);
  if (error != camera::DECODER_ERROR_SUCCESS) {
    ESP_LOGW(TAG, "Frame decode failed: %s", camera::to_string(error));
    return;
  }
  camera::Buffer *buffer = this->decoder_->get_output_buffer();
  if (buffer == nullptr || buffer->get_data_buffer() == nullptr || buffer->get_data_length() == 0) {
    ESP_LOGW(TAG, "The decoded frame is empty");
    return;
  }
  this->last_run_ms_ = now;
  this->has_run_ = true;
  // The decoded spec is the frame size after rotation, which is what the
  // pipeline must sample; a camera frame size configured at build time would
  // not match it.
  if (!this->runtime_->run(buffer->get_data_buffer(), buffer->get_data_length(), spec.width, spec.height))
    ESP_LOGW(TAG, "Inference failed");
}

}  // namespace esphome::tflite_camera

#endif  // USE_TFLITE_MICRO_HELPER
