#include "esphome/core/log.h"
#include "internal_temperature.h"

namespace esphome::internal_temperature {

ESPHOME_LOG_TAG(TAG, "internal_temperature");

void InternalTemperatureSensor::dump_config() { LOG_SENSOR("", "Internal Temperature Sensor", this); }

}  // namespace esphome::internal_temperature
