#pragma once

#include "buffer.h"
#include "camera.h"

namespace esphome::camera {

/// Result codes from the camera processor used to control camera pipeline flow.
enum ProcessorError : uint8_t {
  PROCESSOR_ERROR_SUCCESS = 0,   ///< Processing succeeded, continue pipeline normally.
  PROCESSOR_ERROR_SKIP_FRAME,    ///< Skip current frame, try again with next frame.
  PROCESSOR_ERROR_RETRY_FRAME,   ///< Retry current frame, image needs to be processed multiple times.
  PROCESSOR_ERROR_CONFIGURATION  ///< Fatal config error, shut down pipeline.
};

/// Converts ProcessorError to string.
inline const char *to_string(ProcessorError error) {
  switch (error) {
    case PROCESSOR_ERROR_SUCCESS:
      return "PROCESSOR_ERROR_SUCCESS";
    case PROCESSOR_ERROR_SKIP_FRAME:
      return "PROCESSOR_ERROR_SKIP_FRAME";
    case PROCESSOR_ERROR_RETRY_FRAME:
      return "PROCESSOR_ERROR_RETRY_FRAME";
    case PROCESSOR_ERROR_CONFIGURATION:
      return "PROCESSOR_ERROR_CONFIGURATION";
  }
  return "PROCESSOR_ERROR_INVALID";
}

/// Interface to create a processor that modifies camera images after capture.
/// A Processor can either modify the input image in-place or produce a separate output image.
/// This allows chaining multiple processors (e.g., scaler, colorizer) flexibly.
///
/// For in-place processing:
///   - 'process_pixels()' should operate directly on the input buffer.
///   - 'get_output_image_spec()' should return the same spec as the input.
///   - 'get_output_image()' should return the same image.
///
/// For out-of-place processing (e.g, scaling):
///   - A new image 'Buffer' instance should be allocated during setup.
///   - 'get_output_image_spec()' and 'get_output_image()' should return the new spec and image.
class Processor {
 public:
  /// Get the processor's id for logging, errors and diagnostics.
  virtual const char *get_id() const = 0;

  /// Initialize and configure the processor. Called once before processing starts.
  /// Used to allocate resources and compute any parameters needed for processing.
  /// @return true if setup succeeded, false on failure.
  virtual bool configure() = 0;

  /// Process one uncompressed image frame.
  /// @param input_spec Image specification describing the input data.
  /// @param input Input image buffer to process.
  /// @return ProcessorError indicating processing result.
  virtual ProcessorError process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) = 0;

  /// Process one compressed image frame.
  /// @param input_format Image format describing the input data.
  /// @param input Input image buffer to process.
  /// @return ProcessorError indicating processing result.
  virtual ProcessorError process_compressed_image(camera::ImageFormat input_format, camera::Buffer *input) = 0;

  /// @return the output image specification for uncompressed formats.
  virtual camera::CameraImageSpec *get_output_image_spec() = 0;

  /// Get the output image format.
  /// The format indicates the type of data that subsequent processors in the pipeline will receive.
  /// @return ImageFormat representing the output image type.
  virtual camera::ImageFormat get_output_image_format() = 0;

  /// Get the output image buffer.
  /// For in-place processors this is the same buffer as the input.
  /// @return Pointer to the output image buffer.
  virtual camera::Buffer *get_output_image() = 0;

  /// Release any resources acquired during frame processing.
  /// Called after the entire pipeline has finished processing a frame.
  virtual void release_resources() = 0;

  /// Prints the camera processor's configuration to the log.
  virtual void log_config() = 0;
  virtual ~Processor() = default;
};

}  // namespace esphome::camera
