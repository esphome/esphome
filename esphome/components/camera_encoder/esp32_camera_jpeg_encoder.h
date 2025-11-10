#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32_CAMERA_JPEG_ENCODER

#include <esp_camera.h>

#include "esphome/components/camera/encoder.h"

namespace esphome::camera_encoder {

/// Encoder that uses the software-based JPEG implementation from Espressif's esp32-camera component.
class ESP32CameraJPEGEncoder : public camera::Encoder {
 public:
  /// Constructs a ESP32CameraJPEGEncoder instance.
  /// @param quality Sets the quality of the encoded image (1-100).
  /// @param output Pointer to preallocated output buffer.
  ESP32CameraJPEGEncoder(uint8_t quality, camera::EncoderBuffer *output);
  /// Sets the number of bytes to expand the output buffer on underflow during encoding.
  /// @param buffer_expand_size Number of bytes to expand the buffer.
  void set_buffer_expand_size(size_t buffer_expand_size) { this->buffer_expand_size_ = buffer_expand_size; }
  // -------- Encoder --------
  camera::EncoderError encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) override;
  camera::EncoderBuffer *get_output_buffer() override { return output_; }
  void dump_config() override;
  // -------------------------
 protected:
  static size_t callback(void *arg, size_t index, const void *data, size_t len);
  pixformat_t to_internal_(camera::PixelFormat format);

  camera::EncoderBuffer *output_{};
  size_t buffer_expand_size_{};
  size_t bytes_written_{};
  uint8_t quality_{};
  bool out_of_output_memory_{};
};

}  // namespace esphome::camera_encoder

#endif
