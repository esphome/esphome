#include "template_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
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
  this->set_retry(
      500, 10,
      []() {
        static auto last = millis();
        static int counter = 1;
        ESP_LOGI("RETRY DEMO", "RETRY Count=%u last invocation = %u ms", counter++, millis() - last);
        last = millis();
        if (counter < 6)
          return RETRY;
        else {
          ESP_LOGI("RETRY DEMO", "retry stopped by lambda");
          return DONE;
        }
      },
      2.5f);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
