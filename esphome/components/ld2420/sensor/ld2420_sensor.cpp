#include "ld2420_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2420 {

ESPHOME_LOG_TAG(TAG, "ld2420.sensor");

void LD2420Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Sensor:");
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
}

}  // namespace esphome::ld2420
