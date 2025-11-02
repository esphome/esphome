#pragma once

#ifdef USE_ESP32_VARIANT_ESP32P4

#include "esphome/components/camera/encoder.h"

#include "driver/jpeg_encode.h"

namespace esphome {
namespace camera_encoder {

/// Encoder that uses the hardware-accelerated JPEG engine on ESP32-P4.
class AcceleratedJPEGEncoder : public camera::Encoder {
 public:
  /// Constructs an encoder instance.
  /// @param quality Sets the quality of the encoded image (1-100).
  /// @param subsampling Reduces color information to save memory at the cost of image quality.
  /// @param timeout Timeout in milliseconds before encoding abort.
  AcceleratedJPEGEncoder(uint8_t quality, camera::EncoderSubsampling subsampling, uint16_t timeout);
  // -------- Encoder --------
  void set_output_buffer(camera::DynamicBuffer *output) override { this->output_ = output; }
  camera::EncoderError encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) override;
  camera::Buffer *get_output_buffer() override { return output_; }
  void dump_config() override;
  // -------------------------
 protected:
  jpeg_enc_input_format_t to_internal_(camera::PixelFormat format);
  jpeg_down_sampling_type_t to_internal_(camera::EncoderSubsampling subsampling);
  jpeg_encoder_handle_t encoder_engine_{};

  uint8_t quality_{};
  uint16_t timeout_{};
  bool encoded_first_frame_{};
  camera::EncoderSubsampling subsampling_{};
  camera::DynamicBuffer *output_{};
};

}  // namespace camera_encoder
}  // namespace esphome

#endif
