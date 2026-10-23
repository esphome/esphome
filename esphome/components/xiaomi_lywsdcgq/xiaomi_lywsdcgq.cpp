#include "xiaomi_lywsdcgq.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_lywsdcgq {

ESPHOME_LOG_TAG(TAG, "xiaomi_lywsdcgq");

void XiaomiLYWSDCGQ::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSDCGQ");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiLYWSDCGQ::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(*res.humidity);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_lywsdcgq
