#pragma once

#include <string>
#include <vector>

namespace esphome {
namespace water_heater {

enum WaterHeaterMode {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_HEAT = 1,
  WATER_HEATER_MODE_ECO = 2,
  WATER_HEATER_MODE_BOOST = 3,
};

const char *water_heater_mode_to_string(WaterHeaterMode mode);
bool parse_water_heater_mode(const std::string &str, WaterHeaterMode *out);

}  // namespace water_heater
}  // namespace esphome
