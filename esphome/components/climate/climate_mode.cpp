#include "climate_mode.h"

namespace esphome {
namespace climate {

const char *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return "OFF";
    case CLIMATE_MODE_AUTO:
      return "AUTO";
    case CLIMATE_MODE_COOL:
      return "COOL";
    case CLIMATE_MODE_HEAT:
      return "HEAT";
    case CLIMATE_MODE_FAN_ONLY:
      return "FAN_ONLY";
    case CLIMATE_MODE_DRY:
      return "DRY";
    case CLIMATE_MODE_HEAT_COOL:
      return "HEAT_COOL";
    default:
      return "UNKNOWN";
  }
}
const char *climate_action_to_string(ClimateAction action) {
  switch (action) {
    case CLIMATE_ACTION_OFF:
      return "OFF";
    case CLIMATE_ACTION_COOLING:
      return "COOLING";
    case CLIMATE_ACTION_HEATING:
      return "HEATING";
    case CLIMATE_ACTION_IDLE:
      return "IDLE";
    case CLIMATE_ACTION_DRYING:
      return "DRYING";
    case CLIMATE_ACTION_FAN:
      return "FAN";
    default:
      return "UNKNOWN";
  }
}

const char *climate_fan_mode_to_string(ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_ON:
      return "ON";
    case climate::CLIMATE_FAN_OFF:
      return "OFF";
    case climate::CLIMATE_FAN_AUTO:
      return "AUTO";
    case climate::CLIMATE_FAN_LOW:
      return "LOW";
    case climate::CLIMATE_FAN_MEDIUM:
      return "MEDIUM";
    case climate::CLIMATE_FAN_HIGH:
      return "HIGH";
    case climate::CLIMATE_FAN_MIDDLE:
      return "MIDDLE";
    case climate::CLIMATE_FAN_FOCUS:
      return "FOCUS";
    case climate::CLIMATE_FAN_DIFFUSE:
      return "DIFFUSE";
    default:
      return "UNKNOWN";
  }
}

const char *climate_swing_mode_to_string(ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_OFF:
      return "OFF";
    case climate::CLIMATE_SWING_BOTH:
      return "BOTH";
    case climate::CLIMATE_SWING_VERTICAL:
      return "VERTICAL";
    case climate::CLIMATE_SWING_HORIZONTAL:
      return "HORIZONTAL";
    default:
      return "UNKNOWN";
  }
}

const char *climate_preset_to_string(ClimatePreset preset) {
  switch (preset) {
    case climate::CLIMATE_PRESET_ECO:
      return "ECO";
    case climate::CLIMATE_PRESET_AWAY:
      return "AWAY";
    case climate::CLIMATE_PRESET_BOOST:
      return "BOOST";
    case climate::CLIMATE_PRESET_COMFORT:
      return "COMFORT";
    case climate::CLIMATE_PRESET_HOME:
      return "HOME";
    case climate::CLIMATE_PRESET_SLEEP:
      return "SLEEP";
    case climate::CLIMATE_PRESET_ACTIVITY:
      return "ACTIVITY";
    default:
      return "UNKNOWN";
  }
}

}  // namespace climate
}  // namespace esphome
