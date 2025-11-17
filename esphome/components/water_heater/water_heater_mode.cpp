#include "water_heater_mode.h"

namespace esphome {
namespace water_heater {

const char *water_heater_mode_to_string(WaterHeaterMode mode) {
  switch (mode) {
    case WATER_HEATER_MODE_OFF:
      return "off";
    case WATER_HEATER_MODE_HEAT:
      return "heat";
    case WATER_HEATER_MODE_ECO:
      return "eco";
    case WATER_HEATER_MODE_BOOST:
      return "boost";
    default:
      return "unknown";
  }
}

bool parse_water_heater_mode(const std::string &str, WaterHeaterMode *out) {
  if (str == "off") {
    *out = WATER_HEATER_MODE_OFF;
    return true;
  }
  if (str == "heat") {
    *out = WATER_HEATER_MODE_HEAT;
    return true;
  }
  if (str == "eco") {
    *out = WATER_HEATER_MODE_ECO;
    return true;
  }
  if (str == "boost") {
    *out = WATER_HEATER_MODE_BOOST;
    return true;
  }
  return false;
}

}  // namespace water_heater
}  // namespace esphome
