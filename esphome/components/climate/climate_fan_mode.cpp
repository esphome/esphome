#include "climate_fan_mode.h"

namespace esphome {
namespace climate {

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
}  // namespace climate
}  // namespace esphome
