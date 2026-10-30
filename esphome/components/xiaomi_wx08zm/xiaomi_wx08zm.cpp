#include "xiaomi_wx08zm.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_wx08zm {

ESPHOME_LOG_TAG(TAG, "xiaomi_wx08zm");

void XiaomiWX08ZM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi WX08ZM");
  LOG_BINARY_SENSOR("  ", "Mosquito Repellent", this);
  LOG_SENSOR("  ", "Tablet Resource", this->tablet_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiWX08ZM::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.is_active.has_value()) {
    this->publish_state(*res.is_active);
  }
  if (res.tablet.has_value() && this->tablet_sensor_ != nullptr)
    this->tablet_sensor_->publish_state(*res.tablet);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_wx08zm
