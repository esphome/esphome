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
  publish_optional(this->tablet_sensor_, res.tablet);
  publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_wx08zm
