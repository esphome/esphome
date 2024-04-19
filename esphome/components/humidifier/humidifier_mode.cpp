#include "humidifier_mode.h"

namespace esphome {
namespace humidifier {

const LogString *humidifier_mode_to_string(HumidifierMode mode) {
  switch (mode) {
    case HUMIDIFIER_MODE_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_MODE_LEVEL_1:
      return LOG_STR("LEVEL 1");
    case HUMIDIFIER_MODE_LEVEL_2:
      return LOG_STR("LEVEL 2");
    case HUMIDIFIER_MODE_LEVEL_3:
      return LOG_STR("LEVEL 3");
    case HUMIDIFIER_MODE_PRESET:
      return LOG_STR("PRESET");
    default:
      return LOG_STR("UNKNOWN");
  }
}
const LogString *humidifier_action_to_string(HumidifierAction action) {
  switch (action) {
    case HUMIDIFIER_ACTION_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_ACTION_LEVEL_1:
      return LOG_STR("LEVEL 1");
    case HUMIDIFIER_ACTION_LEVEL_2:
      return LOG_STR("LEVEL 2");
    case HUMIDIFIER_ACTION_LEVEL_3:
      return LOG_STR("LEVEL 3");
    case HUMIDIFIER_ACTION_PRESET:
      return LOG_STR("PRESET");  
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *humidifier_preset_to_string(HumidifierPreset preset) {
  switch (preset) {
    case humidifier::HUMIDIFIER_PRESET_NONE:
      return LOG_STR("NONE");
    case humidifier::HUMIDIFIER_PRESET_CONSTANT_HUMIDITY:
      return LOG_STR("CONSTANT HUMIDITY");
    case humidifier::HUMIDIFIER_PRESET_BABY:
      return LOG_STR("BABY");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace humidifier
}  // namespace esphome