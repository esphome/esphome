#include "ds3231_temperature.h"
#include "esphome/core/log.h"

namespace esphome::ds3231 {

static const char *const TAG = "ds3231.sensor";

void DS3231TemperatureSensor::update() {
  float temperature;
  if (!this->parent_->read_temperature(temperature)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  if (this->fahrenheit_) {
    temperature = temperature * 1.8f + 32.0f;
  }
  this->publish_state(temperature);
}

void DS3231TemperatureSensor::dump_config() { LOG_SENSOR("  ", "DS3231 Temperature", this); }

}  // namespace esphome::ds3231
