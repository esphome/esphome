#include "ld2420_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2420 {

static const char *const TAG = "LD2420.sensor";

void LD2420Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2420 Sensor:");
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
}

}  // namespace ld2420
}  // namespace esphome
