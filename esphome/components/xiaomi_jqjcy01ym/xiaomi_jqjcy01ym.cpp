#include "xiaomi_jqjcy01ym.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_jqjcy01ym {

ESPHOME_LOG_TAG(TAG, "xiaomi_jqjcy01ym");

void XiaomiJQJCY01YM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi JQJCY01YM");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiJQJCY01YM::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(*res.humidity);
  if (res.formaldehyde.has_value() && this->formaldehyde_sensor_ != nullptr)
    this->formaldehyde_sensor_->publish_state(*res.formaldehyde);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_jqjcy01ym
