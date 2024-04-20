#pragma once

#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace humidifier {

/// Enum for all modes a humidifier device can be in.
enum HumidifierMode : uint8_t {
  /// The humidifier device is off
  HUMIDIFIER_MODE_OFF = 0,
  /// The MODE mode is set to Normal
  HUMIDIFIER_MODE_NORMAL = 1,
  /// The MODE mode is set to Eco
  HUMIDIFIER_MODE_ECO = 2,
  /// The MODE mode is set to Away
  HUMIDIFIER_MODE_AWAY = 3,
  /// The MODE mode is set to Boost
  HUMIDIFIER_MODE_BOOST = 4,
  /// The MODE mode is set to Comfort
  HUMIDIFIER_MODE_COMFORT = 5,
  /// The MODE mode is set to Home
  HUMIDIFIER_MODE_HOME = 6,
  /// The MODE mode is set to Sleep
  HUMIDIFIER_MODE_SLEEP = 7,
  /// The MODE mode is set to Auto
  HUMIDIFIER_MODE_AUTO = 8,
  /// The MODE mode is set to Baby
  HUMIDIFIER_MODE_BABY = 9,
};

/// Enum for the current action of the humidifier device. Values match those of Humidifier Mode.
enum HumidifierAction : uint8_t {
  /// The humidifier device is off (inactive or no power)
  HUMIDIFIER_ACTION_OFF = 0,
  /// The ACTION mode is set to Normal
  HUMIDIFIER_ACTION_NORMAL = 1,
  /// The ACTION mode is set to Eco
  HUMIDIFIER_ACTION_ECO = 2,
  /// The ACTION mode is set to Away
  HUMIDIFIER_ACTION_AWAY = 3,
  /// The ACTION mode is set to Boost
  HUMIDIFIER_ACTION_BOOST = 4,
  /// The ACTION mode is set to Comfort
  HUMIDIFIER_ACTION_COMFORT = 5,
  /// The ACTION mode is set to Home
  HUMIDIFIER_ACTION_HOME = 6,
  /// The ACTION mode is set to Sleep
  HUMIDIFIER_ACTION_SLEEP = 7,
  /// The ACTION mode is set to Auto
  HUMIDIFIER_ACTION_AUTO = 8,
  /// The ACTION mode is set to Baby
  HUMIDIFIER_ACTION_BABY = 9,
};

/// Convert the given HumidifierMode to a human-readable string.
const LogString *humidifier_mode_to_string(HumidifierMode mode);

/// Convert the given HumidifierAction to a human-readable string.
const LogString *humidifier_action_to_string(HumidifierAction action);

}  // namespace humidifier
}  // namespace esphome
