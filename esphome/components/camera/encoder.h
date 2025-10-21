#pragma once

#include "buffer.h"
#include "camera.h"

namespace esphome::camera {

/// Result codes from the encoder used to control camera pipeline flow.
enum EncoderError : uint8_t {
  ENCODER_ERROR_SUCCESS = 0,   ///< Encoding succeeded, continue pipeline normally.
  ENCODER_ERROR_SKIP_FRAME,    ///< Skip current frame, try again on next frame.
  ENCODER_ERROR_RETRY_FRAME,   ///< Retry current frame, after buffer growth or for incremental encoding.
  ENCODER_ERROR_CONFIGURATION  ///< Fatal config error, shut down pipeline.
};

/// Converts EncoderError to string.
inline const char *to_string(EncoderError error) {
  switch (error) {
    case ENCODER_ERROR_SUCCESS:
      return "ENCODER_ERROR_SUCCESS";
    case ENCODER_ERROR_SKIP_FRAME:
      return "ENCODER_ERROR_SKIP_FRAME";
    case ENCODER_ERROR_RETRY_FRAME:
      return "ENCODER_ERROR_RETRY_FRAME";
    case ENCODER_ERROR_CONFIGURATION:
      return "ENCODER_ERROR_CONFIGURATION";
  }
  return "ENCODER_ERROR_INVALID";
}

/// Interface for an encoder buffer supporting resizing and variable-length data.
class EncoderBuffer {
 public:
  ///  Sets logical buffer size, reallocates if needed.
  ///  @param size Required size in bytes.
  ///  @return true on success, false on allocation failure.
  virtual bool set_buffer_size(size_t size) = 0;

  /// Returns a pointer to the buffer data.
  virtual uint8_t *get_data() const = 0;

  /// Returns number of bytes currently used.
  virtual size_t get_size() const = 0;

  ///  Returns total allocated buffer size.
  virtual size_t get_max_size() const = 0;

  virtual ~EncoderBuffer() = default;
};

/// Interface for image encoders used in a camera pipeline.
class Encoder {
 public:
  /// Encodes pixel data from a previous camera pipeline stage.
  /// @param spec Specification of the input pixel data.
  /// @param pixels Image pixels in RGB or grayscale format, as specified in @p spec.
  /// @return EncoderError Indicating the result of the encoding operation.
  virtual EncoderError encode_pixels(CameraImageSpec *spec, Buffer *pixels) = 0;

  /// Returns the encoder's output buffer.
  /// @return Pointer to an EncoderBuffer containing encoded data.
  virtual EncoderBuffer *get_output_buffer() = 0;

  ///  Prints the encoder's configuration to the log.
  virtual void dump_config() = 0;
  virtual ~Encoder() = default;
};

}  // namespace esphome::camera
