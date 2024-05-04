#pragma once

#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace humidifier {

/// Enum for all modes a humidifier device can be in.
enum HumidifierMode : uint8_t {
  /// The humidifier device is off
  HUMIDIFIER_MODE_OFF = 0,
  /// The humidifier device is set to humidify/dehumidify to reach the target humidity.
  HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY = 1,
  /// The humidifier device is set to dehumidify mode
  HUMIDIFIER_MODE_DEHUMIDIFY = 2,
  /// The humidifier device is set to humidify to reach the target humidity
  HUMIDIFIER_MODE_HUMIDIFY = 3,
  /** The humidifier device is adjusting the humidity dynamically.
   * For example, the target humidity can be adjusted based on a schedule, or learned behavior.
   * The target humidity can't be adjusted when in this mode.
   */
  HUMIDIFIER_MODE_AUTO = 4
};

/// Enum for the current action of the humidifier device. Values match those of HumidifierMode.
enum HumidifierAction : uint8_t {
  /// The humidifier device is off (inactive or no power)
  HUMIDIFIER_ACTION_OFF = 0,
  /// The humidifier device is actively dehumidifying
  HUMIDIFIER_ACTION_DEHUMIDIFYING = 2,
  /// The humidifier device is actively humidifying
  HUMIDIFIER_ACTION_HUMIDIFYING = 3,
  /// The humidifier device is idle (monitoring humidifier but no action needed)
  HUMIDIFIER_ACTION_IDLE = 4,
};

/// Enum for all modes a humidifier preset can be in
enum HumidifierPreset : uint8_t {
  /// Device is in normal preset
  HUMIDIFIER_PRESET_NORMAL = 0,
  /// Device is in home preset
  HUMIDIFIER_PRESET_HOME = 1,
  /// Device is in away preset
  HUMIDIFIER_PRESET_AWAY = 2,
  /// Device is in boost preset
  HUMIDIFIER_PRESET_BOOST = 3,
  /// Device is in comfort preset
  HUMIDIFIER_PRESET_COMFORT = 4,
  /// Device is running an energy-saving preset
  HUMIDIFIER_PRESET_ECO = 5,
  /// Device is prepared for sleep
  HUMIDIFIER_PRESET_SLEEP = 6,
  /// Device is reacting to activity (e.g., movement sensors)
  HUMIDIFIER_PRESET_ACTIVITY = 7,
};

/// Convert the given HumidifierMode to a human-readable string.
const LogString *humidifier_mode_to_string(HumidifierMode mode);

/// Convert the given HumidifierAction to a human-readable string.
const LogString *humidifier_action_to_string(HumidifierAction action);

/// Convert the given HumidifierSwingMode to a human-readable string.
const LogString *humidifier_preset_to_string(HumidifierPreset preset);

}  // namespace humidifier
}  // namespace esphome
