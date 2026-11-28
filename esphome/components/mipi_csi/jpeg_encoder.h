#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <driver/jpeg_encode.h>

namespace esphome::mipi_csi {

/// ESP32-P4 hardware JPEG encoder with one output buffer: one encoded frame at a time.
class JpegEncoder {
 public:
  /// @param input_format Pixel layout of the frames that will be passed to encode().
  /// @param sub_sample Chroma subsampling to use for the encoded image.
  /// @param quality JPEG quality, 1 (smallest) to 100 (best).
  JpegEncoder(jpeg_enc_input_format_t input_format, jpeg_down_sampling_type_t sub_sample, uint8_t quality) {
    this->config_.src_type = input_format;
    this->config_.sub_sample = sub_sample;
    this->config_.image_quality = quality;
  }

  /// Allocates the encoder engine and its output buffer.
  /// @param width Frame width in pixels.
  /// @param height Frame height in pixels.
  /// @param frame_size Size in bytes of one raw frame.
  /// @return true when the encoder is ready for use.
  bool init(uint16_t width, uint16_t height, size_t frame_size);

  /// Encodes one frame into the internal output buffer.
  /// @param frame Raw pixel data in the configured input format.
  /// @param frame_length Length of @p frame in bytes.
  /// @return Number of JPEG bytes written to the output buffer, or 0 on failure.
  size_t encode(const uint8_t *frame, size_t frame_length);

  /// Returns the buffer holding the most recently encoded image.
  uint8_t *get_output_buffer() const { return this->output_; }
  uint8_t get_quality() const { return this->config_.image_quality; }

 protected:
  jpeg_encoder_handle_t engine_{nullptr};
  jpeg_encode_cfg_t config_{};
  uint8_t *output_{nullptr};
  size_t output_size_{0};
};

}  // namespace esphome::mipi_csi

#endif
