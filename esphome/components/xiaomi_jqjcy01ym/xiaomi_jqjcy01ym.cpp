#include "xiaomi_jqjcy01ym.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_jqjcy01ym {

ESPHOME_LOG_TAG(TAG, "xiaomi_jqjcy01ym");

void XiaomiJQJCY01YM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi JQJCY01YM");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiJQJCY01YM::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(*res.humidity);
  if (res.formaldehyde.has_value() && this->formaldehyde_ != nullptr)
    this->formaldehyde_->publish_state(*res.formaldehyde);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_jqjcy01ym
