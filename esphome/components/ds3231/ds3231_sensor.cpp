#include "ds3231.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231.sensor";

void DS3231Sensor::dump_config() { LOG_SENSOR("", "DS3231 Temperature", this); }

void DS3231Sensor::update() {
  if (!this->parent_->read_temperature_()) {
    ESP_LOGE(TAG, "Failed to read stemperature.");
    return;
  }
  float temp = this->parent_->ds3231_.temp.reg.upper + this->parent_->ds3231_.temp.reg.lower / 100.0f;
  this->publish_state(temp);
}

}  // namespace ds3231
}  // namespace esphome
