#include "ld2420_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2420 {

ESPHOME_LOG_TAG(TAG, "ld2420.binary_sensor");

void LD2420BinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Binary Sensor:");
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_bsensor_);
}

}  // namespace esphome::ld2420
