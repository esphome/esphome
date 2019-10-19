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
};

/// Enum for the current action of the climate device. Values match those of ClimateMode.
enum ClimateAction : uint8_t {
  /// The climate device is off (inactive or no power)
  CLIMATE_ACTION_OFF = 0,
  /// The climate device is actively cooling (usually in cool or auto mode)
  CLIMATE_ACTION_COOLING = 2,
  /// The climate device is actively heating (usually in heat or auto mode)
  CLIMATE_ACTION_HEATING = 3,
};

/// Convert the given ClimateMode to a human-readable string.
const char *climate_mode_to_string(ClimateMode mode);
const char *climate_action_to_string(ClimateAction action);

}  // namespace climate
}  // namespace esphome
