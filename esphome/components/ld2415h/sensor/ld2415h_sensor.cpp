#include "ld2415h_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.sensor";

void LD2415HSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2415H Sensor:");
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
}

}  // namespace ld2415H
}  // namespace esphome
