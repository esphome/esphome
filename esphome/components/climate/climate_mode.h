#pragma once

#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

/// Enum for all modes a climate device can be in.
enum ClimateMode : uint8_t {
  /// The climate device is off
  CLIMATE_MODE_OFF = 0,
  /// The climate device is set to heat/cool to reach the target temperature.
  CLIMATE_MODE_HEAT_COOL = 1,
  /// The climate device is set to cool to reach the target temperature
  CLIMATE_MODE_COOL = 2,
  /// The climate device is set to heat to reach the target temperature
  CLIMATE_MODE_HEAT = 3,
  /// The climate device only has the fan enabled, no heating or cooling is taking place
  CLIMATE_MODE_FAN_ONLY = 4,
  /// The climate device is set to dry/humidity mode
  CLIMATE_MODE_DRY = 5,
  /** The climate device is adjusting the temperatre dynamically.
   * For example, the target temperature can be adjusted based on a schedule, or learned behavior.
   * The target temperature can't be adjusted when in this mode.
   */
  CLIMATE_MODE_AUTO = 6
};

/// Enum for the current action of the climate device. Values match those of ClimateMode.
enum ClimateAction : uint8_t {
  /// The climate device is off (inactive or no power)
  CLIMATE_ACTION_OFF = 0,
  /// The climate device is actively cooling
  CLIMATE_ACTION_COOLING = 2,
  /// The climate device is actively heating
  CLIMATE_ACTION_HEATING = 3,
  /// The climate device is idle (monitoring climate but no action needed)
  CLIMATE_ACTION_IDLE = 4,
  /// The climate device is drying
  CLIMATE_ACTION_DRYING = 5,
  /// The climate device is in fan only mode
  CLIMATE_ACTION_FAN = 6,
};

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
  /// The swing mode is set to Off
  CLIMATE_SWING_OFF = 0,
  /// The fan mode is set to Both
  CLIMATE_SWING_BOTH = 1,
  /// The fan mode is set to Vertical
  CLIMATE_SWING_VERTICAL = 2,
  /// The fan mode is set to Horizontal
  CLIMATE_SWING_HORIZONTAL = 3,
};

/// Enum for all modes a climate swing can be in
enum ClimatePreset : uint8_t {
  /// No preset is active
  CLIMATE_PRESET_NONE = 0,
  /// Device is in home preset
  CLIMATE_PRESET_HOME = 1,
  /// Device is in away preset
  CLIMATE_PRESET_AWAY = 2,
  /// Device is in boost preset
  CLIMATE_PRESET_BOOST = 3,
  /// Device is in comfort preset
  CLIMATE_PRESET_COMFORT = 4,
  /// Device is running an energy-saving preset
  CLIMATE_PRESET_ECO = 5,
  /// Device is prepared for sleep
  CLIMATE_PRESET_SLEEP = 6,
  /// Device is reacting to activity (e.g., movement sensors)
  CLIMATE_PRESET_ACTIVITY = 7,
};

/// Convert the given ClimateMode to a human-readable string.
const LogString *climate_mode_to_string(ClimateMode mode);

/// Convert the given ClimateAction to a human-readable string.
const LogString *climate_action_to_string(ClimateAction action);

/// Convert the given ClimateFanMode to a human-readable string.
const LogString *climate_fan_mode_to_string(ClimateFanMode mode);

/// Convert the given ClimateSwingMode to a human-readable string.
const LogString *climate_swing_mode_to_string(ClimateSwingMode mode);

/// Convert the given ClimateSwingMode to a human-readable string.
const LogString *climate_preset_to_string(ClimatePreset preset);

}  // namespace climate
}  // namespace esphome
