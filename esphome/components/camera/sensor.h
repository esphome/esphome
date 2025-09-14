#pragma once

#include "buffer.h"
#include "camera.h"

#include <optional>

namespace esphome::camera {

/// Interface represents the first stage of the camera pipeline,
/// responsible for capturing image data from the camera sensor.
class Sensor {
 public:
  /// Sets up the camera sensor, configures resolution and applies
  /// sensor-specific settings before image capture can begin.
  /// @return `true` if camera setup succeeded, `false` otherwise.
  virtual bool configure() = 0;

  /// Acquires a frame buffer that contains pixel or encoded data.
  /// Previously acquired buffers must be returned via return_frame_buffer().
  /// @return a captured frame or nullptr if no buffer is available.
  virtual Buffer *acquire_frame_buffer() = 0;

  /// Returns a previously acquired frame buffer back to the sensor for reuse.
  /// @param buffer The buffer obtained from acquire_frame_buffer().
  virtual void return_frame_buffer(Buffer *buffer) = 0;

  /// @return the resolution of the captured frames.
  virtual Resolution get_resolution() = 0;

  /// @return the overall image format.
  virtual ImageFormat get_image_format() = 0;

  /// @return the pixel format if image format is uncompressed (RAW).
  /// @note Only valid when get_image_format() returns IMAGE_FORMAT_RAW.
  virtual std::optional<PixelFormat> get_pixel_format() = 0;

  /// Logs the camera sensor's configuration to the log.
  virtual void log_config() = 0;

  virtual ~Sensor() = default;
};

}  // namespace esphome::camera
