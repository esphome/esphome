#pragma once

#include <string>

namespace esphome::duco::error_lookup {

struct Definition {
  const char *code;
  const char *description;
};

static constexpr Definition ENTRIES[] = {
    {"E01.04.01", "No response from the extractor fan."},
    {"E03.01.01", "ODA temperature sensor value is too low."},
    {"E03.01.02", "ODA temperature sensor value is too high."},
    {"E03.01.03", "ODA temperature sensor cannot be read."},
    {"E03.01.04", "ODA temperature sensor value is unchanged for too long."},
    {"E03.01.05", "ODA temperature sensor is short-circuited."},
    {"E03.01.06", "ODA temperature sensor has a bad connection."},
    {"E04.01.01", "ETA temperature sensor value is too low."},
    {"E04.01.02", "ETA temperature sensor value is too high."},
    {"E04.01.03", "ETA temperature sensor cannot be read."},
    {"E04.01.04", "ETA temperature sensor value is unchanged for too long."},
    {"E04.01.05", "ETA temperature sensor is short-circuited."},
    {"E04.01.06", "ETA temperature sensor has a bad connection."},
    {"E06.01.01", "EHA temperature sensor value is too low."},
    {"E06.01.02", "EHA temperature sensor value is too high."},
    {"E06.01.03", "EHA temperature sensor cannot be read."},
    {"E06.01.04", "EHA temperature sensor value is unchanged for too long."},
    {"E06.01.05", "EHA temperature sensor is short-circuited."},
    {"E06.01.06", "EHA temperature sensor has a bad connection."},
    {"E16.01.01", "Extract air pressure sensor value is too low."},
    {"E16.01.02", "Extract air pressure sensor value is too high."},
    {"E16.01.03", "Extract air pressure sensor cannot be read."},
    {"E16.01.04", "Extract air pressure sensor value is unchanged for too long."},
    {"E16.01.05", "Extract air pressure sensor is short-circuited."},
    {"E16.01.06", "Extract air pressure sensor has a bad connection."},
    {"E28.04.01", "No response from the supply fan."},
    {"E29.01.01", "Supply air pressure sensor value is too low."},
    {"E29.01.02", "Supply air pressure sensor value is too high."},
    {"E29.01.03", "Supply air pressure sensor cannot be read."},
    {"E29.01.04", "Supply air pressure sensor value is unchanged for too long."},
    {"E29.01.05", "Supply air pressure sensor is short-circuited."},
    {"E29.01.06", "Supply air pressure sensor has a bad connection."},
    {"E30.01.01", "SUP temperature sensor value is too low."},
    {"E30.01.02", "SUP temperature sensor value is too high."},
    {"E30.01.03", "SUP temperature sensor cannot be read."},
    {"E30.01.04", "SUP temperature sensor value is unchanged for too long."},
    {"E30.01.05", "SUP temperature sensor is short-circuited."},
    {"E30.01.06", "SUP temperature sensor has a bad connection."},
    {"E31.19.01", "Filters need replacement."},
    {"E32.10.01", "Outdoor temperature is too low; ventilation is off for 8 hours."},
    {"E33.10.01", "Outdoor temperature is too low; ventilation is permanently off."},
    {"E35.17.01", "Temperature sensor value is too low."},
    {"E35.17.02", "Temperature sensor value is too high."},
    {"E35.17.03", "Temperature sensor cannot be read."},
    {"E36.24.01", "Connection to the weather station has been lost."},
    {"E36.24.02", "Connection to the outdoor temperature sensor has been lost."},
    {"E37.17.01", "CO2 sensor value is too low."},
    {"E37.17.02", "CO2 sensor value is too high."},
    {"E37.17.03", "CO2 sensor cannot be read."},
    {"E40.17.01", "Humidity sensor value is too low."},
    {"E40.17.02", "Humidity sensor value is too high."},
    {"E40.17.03", "Humidity sensor cannot be read."},
    {"E42.22.01", "Too many valves are connected."},
    {"E42.22.02", "Too few valves are connected; at least two are required."},
    {"E43.23.01", "Flow adjustment has not happened or failed."},
    {"E43.23.02", "The requested flow rate cannot be achieved."},
    {"E44.06.01", "No connection to the Duco Pre-Heater."},
    {"E44.06.02", "A Duco Pre-Heater is connected but not configured."},
};

inline const Definition *find(const std::string &code) {
  for (const auto &entry : ENTRIES) {
    if (code == entry.code)
      return &entry;
  }
  return nullptr;
}

}  // namespace esphome::duco::error_lookup
