#include "esphome/core/defines.h"

#ifdef USE_ESP32_CAMERA_JPEG_ENCODER

#include "esp32_camera_jpeg_encoder.h"

namespace esphome::camera_encoder {

static const char *const TAG = "camera_encoder";

ESP32CameraJPEGEncoder::ESP32CameraJPEGEncoder(uint8_t quality, camera::EncoderBuffer *output) {
  this->quality_ = quality;
  this->output_ = output;
}

camera::EncoderError ESP32CameraJPEGEncoder::encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) {
  this->bytes_written_ = 0;
  this->out_of_output_memory_ = false;
  bool success = fmt2jpg_cb(pixels->get_data_buffer(), pixels->get_data_length(), spec->width, spec->height,
                            to_internal_(spec->format), this->quality_, callback, this);

  if (!success)
    return camera::ENCODER_ERROR_CONFIGURATION;

  if (this->out_of_output_memory_) {
    if (this->buffer_expand_size_ <= 0)
      return camera::ENCODER_ERROR_SKIP_FRAME;

    size_t current_size = this->output_->get_max_size();
    size_t new_size = this->output_->get_max_size() + this->buffer_expand_size_;
    if (!this->output_->set_buffer_size(new_size)) {
      ESP_LOGE(TAG, "Failed to expand output buffer.");
      this->buffer_expand_size_ = 0;
      return camera::ENCODER_ERROR_SKIP_FRAME;
    }

    ESP_LOGD(TAG, "Output buffer expanded (%u -> %u).", current_size, this->output_->get_max_size());
    return camera::ENCODER_ERROR_RETRY_FRAME;
  }

  this->output_->set_buffer_size(this->bytes_written_);
  return camera::ENCODER_ERROR_SUCCESS;
}

void ESP32CameraJPEGEncoder::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 Camera JPEG Encoder:\n"
                "  Size: %zu\n"
                "  Quality: %d\n"
                "  Expand: %d\n",
                this->output_->get_max_size(), this->quality_, this->buffer_expand_size_);
}

size_t ESP32CameraJPEGEncoder::callback(void *arg, size_t index, const void *data, size_t len) {
  ESP32CameraJPEGEncoder *that = reinterpret_cast<ESP32CameraJPEGEncoder *>(arg);
  uint8_t *buffer = that->output_->get_data();
  size_t buffer_length = that->output_->get_max_size();
  if (index + len > buffer_length) {
    that->out_of_output_memory_ = true;
    return 0;
  }

  std::memcpy(&buffer[index], data, len);
  that->bytes_written_ += len;
  return len;
}

pixformat_t ESP32CameraJPEGEncoder::to_internal_(camera::PixelFormat format) {
  switch (format) {
    case camera::PIXEL_FORMAT_GRAYSCALE:
      return PIXFORMAT_GRAYSCALE;
    case camera::PIXEL_FORMAT_RGB565:
      return PIXFORMAT_RGB565;
    // Internal representation for RGB is in byte order: B, G, R
    case camera::PIXEL_FORMAT_BGR888:
      return PIXFORMAT_RGB888;
  }

  return PIXFORMAT_GRAYSCALE;
}

}  // namespace esphome::camera_encoder

#endif
