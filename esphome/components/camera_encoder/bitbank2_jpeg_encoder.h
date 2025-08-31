#pragma once

#ifdef USE_BITBANK2_JPEG_ENCODER

#include "esphome/components/camera/encoder.h"

#include <JPEGENC.h>

namespace esphome {
namespace camera_encoder {

/// Preset quality levels for JPEG encoding.
enum Bitbank2Quality : uint8_t {
  QUALITY_BEST = 0,  ///< Maximum quality, lowest compression.
  QUALITY_HIGH,      ///< High quality, good balance between size and fidelity.
  QUALITY_MED,       ///< Medium quality, visible artifacts
  QUALITY_LOW        ///< Low quality, highest compression.
};

/// Returns string name for given quality.
inline const char *to_string(Bitbank2Quality quality) {
  switch (quality) {
    case QUALITY_BEST:
      return "QUALITY_BEST";
    case QUALITY_HIGH:
      return "QUALITY_HIGH";
    case QUALITY_MED:
      return "QUALITY_MED";
    case QUALITY_LOW:
      return "QUALITY_LOW";
  }

  return "QUALITY_INVALID";
}

/// Encoder that uses the software-based JPEG implementation from bitbank2 (Larry Bank).
class Bitbank2JPEGEncoder : public camera::Encoder {
 public:
  /// Constructs a DefaultJPEGEncoder instance.
  /// @param quality Sets the quality of the encoded image.
  /// @param subsampling Reduces color information to save memory at the cost of image quality.
  /// @param mcu_count Number of MCUs to encode per call. Zero encodes the full image at once.
  /// @param output Pointer to preallocated output buffer.
  Bitbank2JPEGEncoder(Bitbank2Quality quality, camera::EncoderSubsampling subsampling, size_t mcu_count,
                      camera::EncoderBuffer *output);
  /// Sets the number of bytes to expand the output buffer on underflow during encoding.
  /// @param buffer_expand_size Number of bytes to expand the buffer.
  void set_buffer_expand_size(size_t buffer_expand_size) { this->buffer_expand_size_ = buffer_expand_size; }
  // -------- Encoder --------
  camera::EncoderError encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) override;
  camera::EncoderBuffer *get_output_buffer() override { return output_; }
  void dump_config() override;
  // -------------------------
 protected:
  int to_internal_(Bitbank2Quality quality);
  int to_internal_(camera::PixelFormat format);
  int to_internal_(camera::EncoderSubsampling subsampling);

  Bitbank2Quality quality_{};
  camera::EncoderSubsampling subsampling_{};
  JPEGENC encoder_{};
  JPEGENCODE encoder_state_{};
  bool incremental_{};
  ssize_t mcu_count_{};
  size_t buffer_expand_size_{};
  camera::EncoderBuffer *output_{};
};

}  // namespace camera_encoder
}  // namespace esphome

#endif
