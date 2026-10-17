#include "esphome/core/log.h"
#include "teleinfo_sensor.h"

namespace esphome::teleinfo {

ESPHOME_LOG_TAG(TAG, "teleinfo_sensor");
TeleInfoSensor::TeleInfoSensor(const char *tag) { this->tag = std::string(tag); }
void TeleInfoSensor::publish_val(const std::string &val) {
  auto newval = parse_number<float>(val);
  if (!newval.has_value()) {
    // Publishing 0 instead would look like a meter reset to total_increasing energy sensors
    ESP_LOGW(TAG, "Value '%s' for tag %s is not a number", val.c_str(), this->tag.c_str());
    return;
  }
  this->publish_state(*newval);
}
void TeleInfoSensor::dump_config() { LOG_SENSOR("  ", "Teleinfo Sensor", this); }
}  // namespace esphome::teleinfo
