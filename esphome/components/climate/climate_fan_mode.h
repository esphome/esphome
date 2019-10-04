#pragma once

#include <cstdint>

namespace esphome {
namespace climate {

/// Enum for all modes a climate fan can be in
enum ClimateFanMode : uint8_t {
  /// The fan mode is set to On
  CLIMATE_FAN_ON = 0,
  /// The fan mode is set to Off
  CLIMATE_FAN_OFF = 1,
  /// The fan mode is set to Auto
  CLIMATE_FAN_AUTO = 2,
  /// The fan mode is set to Low
  CLIMATE_FAN_LOW = 3,
  /// The fan mode is set to Medium
  CLIMATE_FAN_MEDIUM = 4,
  /// The fan mode is set to High
  CLIMATE_FAN_HIGH = 5,
  /// The fan mode is set to Middle
  CLIMATE_FAN_MIDDLE = 6,
  /// The fan mode is set to Focus
  CLIMATE_FAN_FOCUS = 7,
  /// The fan mode is set to Diffuse
  CLIMATE_FAN_DIFFUSE = 8,
};

/// Convert the given ClimateFanMode to a human-readable string.
const char *climate_fan_mode_to_string(ClimateFanMode mode);

}  // namespace climate
}  // namespace esphome
