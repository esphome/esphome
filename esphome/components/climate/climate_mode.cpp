#include "climate_mode.h"

namespace esphome {
namespace climate {

const LogString *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return LOG_STR("OFF");
    case CLIMATE_MODE_HEAT_COOL:
      return LOG_STR("HEAT_COOL");
    case CLIMATE_MODE_AUTO:
      return LOG_STR("AUTO");
    case CLIMATE_MODE_COOL:
      return LOG_STR("COOL");
    case CLIMATE_MODE_HEAT:
      return LOG_STR("HEAT");
    case CLIMATE_MODE_FAN_ONLY:
      return LOG_STR("FAN_ONLY");
    case CLIMATE_MODE_DRY:
      return LOG_STR("DRY");
    default:
      return LOG_STR("UNKNOWN");
  }
}
const LogString *climate_action_to_string(ClimateAction action) {
  switch (action) {
    case CLIMATE_ACTION_OFF:
      return LOG_STR("OFF");
    case CLIMATE_ACTION_COOLING:
      return LOG_STR("COOLING");
    case CLIMATE_ACTION_HEATING:
      return LOG_STR("HEATING");
    case CLIMATE_ACTION_IDLE:
      return LOG_STR("IDLE");
    case CLIMATE_ACTION_DRYING:
      return LOG_STR("DRYING");
    case CLIMATE_ACTION_FAN:
      return LOG_STR("FAN");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *climate_fan_mode_to_string(ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_ON:
      return LOG_STR("ON");
    case climate::CLIMATE_FAN_OFF:
      return LOG_STR("OFF");
    case climate::CLIMATE_FAN_AUTO:
      return LOG_STR("AUTO");
    case climate::CLIMATE_FAN_LOW:
      return LOG_STR("LOW");
    case climate::CLIMATE_FAN_MEDIUM:
      return LOG_STR("MEDIUM");
    case climate::CLIMATE_FAN_HIGH:
      return LOG_STR("HIGH");
    case climate::CLIMATE_FAN_MIDDLE:
      return LOG_STR("MIDDLE");
    case climate::CLIMATE_FAN_FOCUS:
      return LOG_STR("FOCUS");
    case climate::CLIMATE_FAN_DIFFUSE:
      return LOG_STR("DIFFUSE");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *climate_swing_mode_to_string(ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_OFF:
      return LOG_STR("OFF");
    case climate::CLIMATE_SWING_BOTH:
      return LOG_STR("BOTH");
    case climate::CLIMATE_SWING_VERTICAL:
      return LOG_STR("VERTICAL");
    case climate::CLIMATE_SWING_HORIZONTAL:
      return LOG_STR("HORIZONTAL");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *climate_preset_to_string(ClimatePreset preset) {
  switch (preset) {
    case climate::CLIMATE_PRESET_NONE:
      return LOG_STR("NONE");
    case climate::CLIMATE_PRESET_HOME:
      return LOG_STR("HOME");
    case climate::CLIMATE_PRESET_ECO:
      return LOG_STR("ECO");
    case climate::CLIMATE_PRESET_AWAY:
      return LOG_STR("AWAY");
    case climate::CLIMATE_PRESET_BOOST:
      return LOG_STR("BOOST");
    case climate::CLIMATE_PRESET_COMFORT:
      return LOG_STR("COMFORT");
    case climate::CLIMATE_PRESET_SLEEP:
      return LOG_STR("SLEEP");
    case climate::CLIMATE_PRESET_ACTIVITY:
      return LOG_STR("ACTIVITY");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace climate
}  // namespace esphome
