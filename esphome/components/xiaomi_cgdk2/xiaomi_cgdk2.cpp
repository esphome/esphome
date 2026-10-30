#include "xiaomi_cgdk2.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgdk2 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgdk2");

void XiaomiCGDK2::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGDK2");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiCGDK2::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(*res.humidity);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_cgdk2
