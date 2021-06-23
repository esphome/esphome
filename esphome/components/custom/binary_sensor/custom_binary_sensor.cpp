#include "custom_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace custom {

static const char *const TAG = "custom.binary_sensor";

void CustomBinarySensorConstructor::dump_config() {
  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("", "Custom Binary Sensor", child);
  }
}

}  // namespace custom
}  // namespace esphome
