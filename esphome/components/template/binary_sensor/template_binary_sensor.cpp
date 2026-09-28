#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.binary_sensor";

void TemplateBinarySensor::setup() {
  if (!this->f_.has_value()) {
    this->disable_loop();
  } else {
    this->loop();
  }
}

void TemplateBinarySensor::loop() {
  auto s = this->f_();
  if (s.has_value()) {
    this->publish_state(*s);
  }
}

void TemplateBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Template Binary Sensor", this); }

}  // namespace esphome::template_
