#pragma once

#include "buffer.h"
#include "camera.h"

namespace esphome::camera {

/// Result codes from a decoder used to control camera pipeline flow.
enum DecoderError : uint8_t {
  DECODER_ERROR_SUCCESS = 0,    ///< Decoding succeeded.
  DECODER_ERROR_FAILED,         ///< Decoding failed.
  DECODER_ERROR_UNSUPPORTED,    ///< Format or configuration not supported.
  DECODER_ERROR_OUT_OF_MEMORY,  ///< The output buffer could not be allocated.
};

/// Converts DecoderError to string.
inline const char *to_string(DecoderError error) {
  switch (error) {
    case DECODER_ERROR_SUCCESS:
      return "DECODER_ERROR_SUCCESS";
    case DECODER_ERROR_FAILED:
      return "DECODER_ERROR_FAILED";
    case DECODER_ERROR_UNSUPPORTED:
      return "DECODER_ERROR_UNSUPPORTED";
    case DECODER_ERROR_OUT_OF_MEMORY:
      return "DECODER_ERROR_OUT_OF_MEMORY";
  }
  return "DECODER_ERROR_INVALID";
}

/// Interface for image decoders used in a camera pipeline.
///
/// The decoder owns its output buffer, mirroring the way an Encoder owns its
/// EncoderBuffer, so a consumer can decode repeatedly without allocating.
class Decoder {
 public:
  /// Decodes an encoded image into the decoder's output buffer.
  /// @param data Encoded image bytes.
  /// @param length Number of encoded bytes.
  /// @param spec Filled with the decoded width, height and pixel format.
  virtual DecoderError decode(const uint8_t *data, size_t length, CameraImageSpec *spec) = 0;

  /// Returns the decoder's output buffer, or nullptr before a successful decode.
  /// The pointer and its contents are valid only until the next call to decode().
  virtual Buffer *get_output_buffer() = 0;

  virtual ~Decoder() = default;
};

}  // namespace esphome::camera
