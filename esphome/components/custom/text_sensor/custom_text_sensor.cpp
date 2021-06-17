#include "custom_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace custom {

static const char *const TAG = "custom.text_sensor";

void CustomTextSensorConstructor::dump_config() {
  for (auto *child : this->text_sensors_) {
    LOG_TEXT_SENSOR("", "Custom Text Sensor", child);
  }
}

}  // namespace custom
}  // namespace esphome
