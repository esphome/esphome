#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.binary_sensor";

void TemplateBinarySensor::loop() {
  if (this->f_.has_value()) {
    auto s = (*this->f_)();
    if (s.has_value()) {
      this->publish_state(*s);
    }
  } else if (this->has_state()) {
    this->publish_state(this->state);
  }
}
void TemplateBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Template Binary Sensor", this); }

}  // namespace template_
}  // namespace esphome
