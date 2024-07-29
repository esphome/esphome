#include "ld2420_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2420 {

static const char *const TAG = "LD2420.binary_sensor";

void LD2420BinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2420 BinarySensor:");
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_bsensor_);
}

}  // namespace ld2420
}  // namespace esphome
