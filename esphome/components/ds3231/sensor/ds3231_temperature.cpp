#include "ds3231_temperature.h"
#include "esphome/core/log.h"

namespace esphome::ds3231 {

ESPHOME_LOG_TAG(TAG, "ds3231.sensor");

void DS3231TemperatureSensor::update() {
  float temperature;
  if (!this->parent_->read_temperature(temperature)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->publish_state(temperature);
}

void DS3231TemperatureSensor::dump_config() {
  LOG_SENSOR("", "DS3231 Temperature", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::ds3231
