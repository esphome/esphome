#include "custom_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace custom {

static const char *const TAG = "custom.sensor";

void CustomSensorConstructor::dump_config() {
  for (auto *child : this->sensors_) {
    LOG_SENSOR("", "Custom Sensor", child);
  }
}

}  // namespace custom
}  // namespace esphome
