#include "humidifier_mode.h"

namespace esphome {
namespace humidifier {

const LogString *humidifier_mode_to_string(HumidifierMode mode) {
  switch (mode) {
    case HUMIDIFIER_MODE_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY:
      return LOG_STR("HUMIDIFY_DEHUMIDIFY");
    case HUMIDIFIER_MODE_AUTO:
      return LOG_STR("AUTO");
    case HUMIDIFIER_MODE_HUMIDIFY:
      return LOG_STR("HUMIDIFY");
    case HUMIDIFIER_MODE_DEHUMIDIFY:
      return LOG_STR("DEHUMIDIFY");
    default:
      return LOG_STR("UNKNOWN");
  }
}
const LogString *humidifier_action_to_string(HumidifierAction action) {
  switch (action) {
    case HUMIDIFIER_ACTION_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_ACTION_HUMIDIFYING:
      return LOG_STR("HUMIDIFYING");
    case HUMIDIFIER_ACTION_IDLE:
      return LOG_STR("IDLE");
    case HUMIDIFIER_ACTION_DEHUMIDIFYING:
      return LOG_STR("DEHUMIDIFYING");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *humidifier_preset_to_string(HumidifierPreset preset) {
  switch (preset) {
    case humidifier::HUMIDIFIER_PRESET_NORMAL:
      return LOG_STR("NORMAL");
    case humidifier::HUMIDIFIER_PRESET_HOME:
      return LOG_STR("HOME");
    case humidifier::HUMIDIFIER_PRESET_ECO:
      return LOG_STR("ECO");
    case humidifier::HUMIDIFIER_PRESET_AWAY:
      return LOG_STR("AWAY");
    case humidifier::HUMIDIFIER_PRESET_BOOST:
      return LOG_STR("BOOST");
    case humidifier::HUMIDIFIER_PRESET_COMFORT:
      return LOG_STR("COMFORT");
    case humidifier::HUMIDIFIER_PRESET_SLEEP:
      return LOG_STR("SLEEP");
    case humidifier::HUMIDIFIER_PRESET_ACTIVITY:
      return LOG_STR("ACTIVITY");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace humidifier
}  // namespace esphome
