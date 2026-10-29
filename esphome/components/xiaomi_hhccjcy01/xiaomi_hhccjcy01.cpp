#include "xiaomi_hhccjcy01.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_hhccjcy01 {

ESPHOME_LOG_TAG(TAG, "xiaomi_hhccjcy01");

void XiaomiHHCCJCY01::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCJCY01");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Moisture", this->moisture_sensor_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiHHCCJCY01::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(*res.temperature);
  if (res.moisture.has_value() && this->moisture_sensor_ != nullptr)
    this->moisture_sensor_->publish_state(*res.moisture);
  if (res.conductivity.has_value() && this->conductivity_sensor_ != nullptr)
    this->conductivity_sensor_->publish_state(*res.conductivity);
  if (res.illuminance.has_value() && this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(*res.illuminance);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_hhccjcy01
