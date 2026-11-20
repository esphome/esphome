#include "systa_bus_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::systa_bus {

// Indexed by AquaSensor
PROGMEM_STRING_TABLE(AquaSensorNames, "Temperature TSA", "Temperature TSE", "Temperature TWU", "Temperature TW2",
                     "Pump Speed");

void dump_aqua_sensor(const char *log_tag, sensor::Sensor *sensor, AquaSensor which) {
  sensor::log_sensor(log_tag, "  ", LOG_STR_ARG(AquaSensorNames::get_log_str(static_cast<uint8_t>(which), 0)), sensor);
}

}  // namespace esphome::systa_bus
