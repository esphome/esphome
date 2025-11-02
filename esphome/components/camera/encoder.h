#pragma once

#include "camera.h"
#include "dynamic_buffer.h"

namespace esphome::camera {

/// Chroma subsampling modes used by the encoder.
/// Defines the ratio of color (chroma) resolution to luma resolution.
enum EncoderSubsampling : uint8_t {
  SUBSAMPLING_444 = 0,  ///< Full chroma resolution.
  SUBSAMPLING_422,      ///< Horizontal chroma subsampling.
  SUBSAMPLING_420       ///< Horizontal and vertical chroma subsampling.
};

/// Result codes from the encoder used to control camera pipeline flow.
enum EncoderError : uint8_t {
  ENCODER_ERROR_SUCCESS = 0,   ///< Encoding succeeded, continue pipeline normally.
  ENCODER_ERROR_SKIP_FRAME,    ///< Skip current frame, try again on next frame.
  ENCODER_ERROR_RETRY_FRAME,   ///< Retry current frame, after buffer growth or for incremental encoding.
  ENCODER_ERROR_CONFIGURATION  ///< Fatal config error, shut down pipeline.
};

/// Returns string name for given EncoderSubsampling.
inline const char *to_string(EncoderSubsampling subsampling) {
  switch (subsampling) {
    case SUBSAMPLING_444:
      return "SUBSAMPLING_444";
    case SUBSAMPLING_422:
      return "SUBSAMPLING_422";
    case SUBSAMPLING_420:
      return "SUBSAMPLING_420";
  }
  return "SUBSAMPLING_INVALID";
}

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

/// Interface for image encoders used in a camera pipeline.
class Encoder {
 public:
  /// Sets the encoder's output buffer.
  virtual void set_output_buffer(DynamicBuffer *buffer) = 0;

  /// Encodes pixel data from a previous camera pipeline stage.
  /// @param spec Specification of the input pixel data.
  /// @param pixels Image pixels in RGB or grayscale format, as specified in @p spec.
  /// @return EncoderError Indicating the result of the encoding operation.
  virtual EncoderError encode_pixels(CameraImageSpec *spec, Buffer *pixels) = 0;

  /// Returns the encoder's output buffer.
  /// @return Pointer to a Buffer containing encoded data.
  virtual Buffer *get_output_buffer() = 0;

  ///  Prints the encoder's configuration to the log.
  virtual void dump_config() = 0;
  virtual ~Encoder() = default;
};

}  // namespace esphome::camera
