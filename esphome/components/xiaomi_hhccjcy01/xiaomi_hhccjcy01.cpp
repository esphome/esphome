#include "xiaomi_hhccjcy01.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_hhccjcy01 {

ESPHOME_LOG_TAG(TAG, "xiaomi_hhccjcy01");

void XiaomiHHCCJCY01::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCJCY01");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiHHCCJCY01::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.moisture.has_value() && this->moisture_ != nullptr)
    this->moisture_->publish_state(*res.moisture);
  if (res.conductivity.has_value() && this->conductivity_ != nullptr)
    this->conductivity_->publish_state(*res.conductivity);
  if (res.illuminance.has_value() && this->illuminance_ != nullptr)
    this->illuminance_->publish_state(*res.illuminance);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_hhccjcy01
