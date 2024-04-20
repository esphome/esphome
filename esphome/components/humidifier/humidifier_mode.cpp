#include "humidifier_mode.h"

namespace esphome {
namespace humidifier {

const LogString *humidifier_mode_to_string(HumidifierMode mode) {
  switch (mode) {
    case HUMIDIFIER_MODE_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_MODE_NORMAL:
      return LOG_STR("NORMAL");
    case HUMIDIFIER_MODE_ECO:
      return LOG_STR("ECO");
    case HUMIDIFIER_MODE_AWAY:
      return LOG_STR("AWAY");
    case HUMIDIFIER_MODE_BOOST:
      return LOG_STR("BOOST");
    case HUMIDIFIER_MODE_COMFORT:
      return LOG_STR("COMFORT");
    case HUMIDIFIER_MODE_HOME:
      return LOG_STR("HOME");
    case HUMIDIFIER_MODE_SLEEP:
      return LOG_STR("SLEEP");
    case HUMIDIFIER_MODE_AUTO:
      return LOG_STR("AUTO");
    case HUMIDIFIER_MODE_BABY:
      return LOG_STR("BABY");
    default:
      return LOG_STR("UNKNOWN");
  }
}
const LogString *humidifier_action_to_string(HumidifierAction action) {
  switch (action) {
    case HUMIDIFIER_ACTION_OFF:
      return LOG_STR("OFF");
    case HUMIDIFIER_ACTION_IDLE:
      return LOG_STR("IDLE");
    case HUMIDIFIER_ACTION_HUMIDIFYING:
      return LOG_STR("HUMIDIFYING");
    case HUMIDIFIER_ACTION_DRYING:
      return LOG_STR("DRYING");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace humidifier
}  // namespace esphome
