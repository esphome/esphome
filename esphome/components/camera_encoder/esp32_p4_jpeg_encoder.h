#pragma once

#ifdef USE_P4_CAMERA_JPEG_ENCODER

#include "driver/jpeg_encode.h"

#include "esphome/components/camera/encoder.h"

namespace esphome {
namespace camera_encoder {

/// Encoder that uses the hardware-accelerated JPEG engine on ESP32-P4.
class ESP32P4JPEGEncoder : public camera::Encoder {
 public:
  ESP32P4JPEGEncoder(uint8_t quality, camera::EncoderSubsampling subsampling, uint16_t timeout,
                     camera::EncoderBuffer *output);
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }
  // -------- Encoder --------
  camera::EncoderError encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) override;
  camera::EncoderBuffer *get_output_buffer() override { return output_; }
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
  camera::EncoderBuffer *output_{};
};

}  // namespace camera_encoder
}  // namespace esphome

#endif
