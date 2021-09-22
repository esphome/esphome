#include "template_sensor.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace template_ {

static const char *const TAG = "template.sensor";

void TemplateSensor::update() {
  if (this->f_.has_value()) {
    auto val = (*this->f_)();
    if (val.has_value()) {
      this->publish_state(*val);
    }
  } else if (!std::isnan(this->get_raw_state())) {
    this->publish_state(this->get_raw_state());
  }
}
float TemplateSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateSensor::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void TemplateSensor::dump_config() {
  LOG_SENSOR("", "Template Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
