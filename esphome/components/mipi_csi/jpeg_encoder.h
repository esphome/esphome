#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <driver/jpeg_encode.h>

namespace esphome::mipi_csi {

/// Wrapper around the ESP32-P4 hardware JPEG encoder.
///
/// The encoder owns a single output buffer sized for the configured frame geometry, so only one
/// encoded frame can be in flight at a time. The caller must therefore not start a new encode while
/// a previously returned buffer is still being read.
class JpegEncoder {
 public:
  /// Allocates the encoder engine and its output buffer.
  /// @param width Frame width in pixels.
  /// @param height Frame height in pixels.
  /// @param input_format Pixel layout of the frames that will be passed to encode().
  /// @param sub_sample Chroma subsampling to use for the encoded image.
  /// @param quality JPEG quality, 1 (smallest) to 100 (best).
  /// @return true when the encoder is ready for use.
  bool init(uint16_t width, uint16_t height, jpeg_enc_input_format_t input_format, jpeg_down_sampling_type_t sub_sample,
            uint8_t quality);

  /// Encodes one frame into the internal output buffer.
  /// @param frame Raw pixel data in the configured input format.
  /// @param frame_length Length of @p frame in bytes.
  /// @return Number of JPEG bytes written to the output buffer, or 0 on failure.
  size_t encode(const uint8_t *frame, size_t frame_length);

  /// Returns the buffer holding the most recently encoded image.
  uint8_t *get_output_buffer() const { return this->output_; }

  ~JpegEncoder();

 protected:
  jpeg_encoder_handle_t engine_{nullptr};
  jpeg_encode_cfg_t config_{};
  uint8_t *output_{nullptr};
  size_t output_size_{0};
};

}  // namespace esphome::mipi_csi

#endif
