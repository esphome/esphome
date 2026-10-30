#include "xiaomi_mhoc303.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mhoc303 {

ESPHOME_LOG_TAG(TAG, "xiaomi_mhoc303");

void XiaomiMHOC303::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MHOC303");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiMHOC303::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(*res.humidity);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_mhoc303
