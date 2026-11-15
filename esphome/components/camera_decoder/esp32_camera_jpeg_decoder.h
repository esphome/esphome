#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CAMERA_DECODER
// esp_new_jpeg is an ESP-IDF managed component, so the decoder is IDF-only.
#ifdef USE_ESP_IDF

#include "esphome/components/camera/decoder.h"

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#include <memory>

namespace esphome::camera_decoder {

/// Output buffer owned by the decoder. esp_new_jpeg writes into a 16-byte
/// aligned block, so this buffer uses the library's allocator.
class JpegBuffer final : public camera::Buffer {
 public:
  explicit JpegBuffer(size_t capacity);
  ~JpegBuffer() override;

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t get_data_length() override { return this->length_; }
  bool valid() const { return this->data_ != nullptr; }
  size_t capacity() const { return this->capacity_; }
  void set_length(size_t length) { this->length_ = length; }

 protected:
  size_t capacity_;
  size_t length_{0};
  uint8_t *data_{nullptr};
};

/// JPEG decoder backed by espressif/esp_new_jpeg.
class ESP32CameraJPEGDecoder final : public camera::Decoder {
 public:
  ESP32CameraJPEGDecoder(camera::PixelFormat output_format, int rotation);
  ~ESP32CameraJPEGDecoder() override;

  camera::DecoderError decode(const uint8_t *data, size_t length, camera::CameraImageSpec *spec) override;
  /// Returns nullptr until a frame has been decoded successfully; the pointer
  /// and its contents stay valid only until the next decode().
  camera::Buffer *get_output_buffer() override { return this->has_output_ ? this->output_.get() : nullptr; }

 protected:
  /// Opens the decoder on first use; the handle is kept for later frames.
  bool open_decoder_();

  camera::PixelFormat output_format_;
  jpeg_rotate_t rotation_;
  jpeg_dec_handle_t decoder_{nullptr};
  bool has_output_{false};
  std::unique_ptr<JpegBuffer> output_;
};

}  // namespace esphome::camera_decoder

#endif  // USE_ESP_IDF
#endif  // USE_CAMERA_DECODER
