#pragma once

#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace humidifier {

/// Enum for all modes a humidifier device can be in.
enum HumidifierMode : uint8_t {
  /// The humid device is off
  HUMIDIFIER_MODE_OFF = 0,
  /// The MODE mode is set to Level 1
  HUMIDIFIER_MODE_LEVEL_1 = 1,
  /// The MODE mode is set to Level 2
  HUMIDIFIER_MODE_LEVEL_2 = 2,
  /// The MODE mode is set to Level 3
  HUMIDIFIER_MODE_LEVEL_3 = 3,
  /// The MODE mode is set to preset mode
  HUMIDIFIER_MODE_PRESET = 4,

};

/// Enum for the current action of the humidifier device. Values match those of HumidifierMode.
enum HumidifierAction : uint8_t {
  /// The humidifier device is off (inactive or no power)
  HUMIDIFIER_ACTION_OFF = 0,
  /// The ACTION mode is set to Level 1
  HUMIDIFIER_ACTION_LEVEL_1 = 1,
  /// The ACTION mode is set to Level 2
  HUMIDIFIER_ACTION_LEVEL_2 = 2,
  /// The ACTION mode is set to Level 3
  HUMIDIFIER_ACTION_LEVEL_3 = 3,
  /// The ACTION mode is set to preset mode
  HUMIDIFIER_ACTION_PRESET = 4,
};

/// Enum for all preset modes
enum HumidifierPreset : uint8_t {
  /// No preset is active
  HUMIDIFIER_PRESET_NONE = 0,
  /// Device is in constant humidifier
  HUMIDIFIER_PRESET_CONSTANT_HUMIDITY = 1,
  /// Device is in baby preset
  HUMIDIFIER_PRESET_BABY = 2,

};

/// Convert the given HumidifierMode to a human-readable string.
const LogString *humidifier_mode_to_string(HumidifierMode mode);

/// Convert the given HumidifierAction to a human-readable string.
const LogString *humidifier_action_to_string(HumidifierAction action);

/// Convert the given PresetMode to a human-readable string.
const LogString *humidifier_preset_to_string(HumidifierPreset preset);

}  // namespace humidifier
}  // namespace esphome
