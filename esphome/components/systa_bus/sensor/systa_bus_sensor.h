#pragma once

#include "esphome/components/sensor/sensor.h"

#include <cstdint>

namespace esphome::systa_bus {

enum class AquaSensor : uint8_t {
  AQUA_SENSOR_TEMPERATURE_TSA,
  AQUA_SENSOR_TEMPERATURE_TSE,
  AQUA_SENSOR_TEMPERATURE_TWU,
  AQUA_SENSOR_TEMPERATURE_TW2,
  AQUA_SENSOR_PUMP_SPEED,
};

void dump_aqua_sensor(const char *log_tag, sensor::Sensor *sensor, AquaSensor which);

}  // namespace esphome::systa_bus
