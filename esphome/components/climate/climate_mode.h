#pragma once

#include <cstdint>

namespace esphome {
namespace climate {

/// Enum for all modes a climate device can be in.
enum ClimateMode : uint8_t {
  /// The climate device is off (not in auto, heat or cool mode)
  CLIMATE_MODE_OFF = 0,
  /// The climate device is set to automatically change the heating/cooling cycle
  CLIMATE_MODE_AUTO = 1,
  /// The climate device is manually set to cool mode (not in auto mode!)
  CLIMATE_MODE_COOL = 2,
  /// The climate device is manually set to heat mode (not in auto mode!)
  CLIMATE_MODE_HEAT = 3,
  /// The climate device is manually set to fan only mode
  CLIMATE_MODE_FAN_ONLY = 4,
  /// The climate device is manually set to dry mode
  CLIMATE_MODE_DRY = 5,
};

/// Enum for the current action of the climate device. Values match those of ClimateMode.
enum ClimateAction : uint8_t {
  /// The climate device is off (inactive or no power)
  CLIMATE_ACTION_OFF = 0,
  /// The climate device is actively cooling (usually in cool or auto mode)
  CLIMATE_ACTION_COOLING = 2,
  /// The climate device is actively heating (usually in heat or auto mode)
  CLIMATE_ACTION_HEATING = 3,
  /// The climate device is idle (monitoring climate but no action needed)
  CLIMATE_ACTION_IDLE = 4,
  /// The climate device is drying (either mode DRY or AUTO)
  CLIMATE_ACTION_DRYING = 5,
  /// The climate device is in fan only mode (either mode FAN_ONLY or AUTO)
  CLIMATE_ACTION_FAN = 6,
};

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

/// Enum for all modes a climate swing can be in
enum ClimateSwingMode : uint8_t {
  /// The sing mode is set to Off
  CLIMATE_SWING_OFF = 0,
  /// The fan mode is set to Both
  CLIMATE_SWING_BOTH = 1,
  /// The fan mode is set to Vertical
  CLIMATE_SWING_VERTICAL = 2,
  /// The fan mode is set to Horizontal
  CLIMATE_SWING_HORIZONTAL = 3,
};

/// Convert the given ClimateMode to a human-readable string.
const char *climate_mode_to_string(ClimateMode mode);

/// Convert the given ClimateAction to a human-readable string.
const char *climate_action_to_string(ClimateAction action);

/// Convert the given ClimateFanMode to a human-readable string.
const char *climate_fan_mode_to_string(ClimateFanMode mode);

/// Convert the given ClimateSwingMode to a human-readable string.
const char *climate_swing_mode_to_string(ClimateSwingMode mode);

}  // namespace climate
}  // namespace esphome
