#pragma once

#include "buffer.h"
#include "camera.h"

namespace esphome {
namespace camera {

/// Result codes from the sensor used to control camera pipeline flow.
enum SensorError : uint8_t {
  SENSOR_ERROR_SUCCESS = 0,   ///< Capture succeeded, continue pipeline normally.
  SENSOR_ERROR_CONFIGURATION  ///< Fatal config error, shut down pipeline.
};

/// Converts SensorError to string.
inline const char *to_string(SensorError error) {
  switch (error) {
    case SENSOR_ERROR_SUCCESS:
      return "SENSOR_ERROR_SUCCESS";
    case SENSOR_ERROR_CONFIGURATION:
      return "SENSOR_ERROR_CONFIGURATION";
  }
  return "SENSOR_ERROR_INVALID";
}

/// Interface represents the first stage of the camera pipeline,
/// responsible for capturing raw image data from the camera sensor.
class Sensor {
 public:
  /// Captures raw pixels from the camera sensor.
  /// @return SensorError Indicating the result of the capture operation.
  virtual SensorError capture_pixels() = 0;

  /// Returns the sensor's current image buffer.
  /// @return Pointer to a Buffer containing the last captured frame.
  virtual Buffer *get_image_buffer() = 0;

  /// Returns the camera's image specification.
  virtual CameraImageSpec *get_image_spec() = 0;

  /// Prints the camera sensor's configuration to the log.
  virtual void dump_config() = 0;
  virtual ~Sensor() = default;
};

}  // namespace camera
}  // namespace esphome
