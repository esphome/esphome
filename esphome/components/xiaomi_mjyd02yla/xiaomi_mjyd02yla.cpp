#include "xiaomi_mjyd02yla.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mjyd02yla {

ESPHOME_LOG_TAG(TAG, "xiaomi_mjyd02yla");

void XiaomiMJYD02YLA::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MJYD02YL-A");
  this->dump_bindkey_(TAG);
  LOG_BINARY_SENSOR("  ", "Motion", this);
  LOG_BINARY_SENSOR("  ", "Light", this->light_binary_sensor_);
  LOG_SENSOR("  ", "Idle Time", this->idle_time_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
}

void XiaomiMJYD02YLA::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.idle_time.has_value() && this->idle_time_sensor_ != nullptr)
    this->idle_time_sensor_->publish_state(*res.idle_time);
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
  if (res.illuminance.has_value() && this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(*res.illuminance);
  if (res.is_light.has_value() && this->light_binary_sensor_ != nullptr)
    this->light_binary_sensor_->publish_state(*res.is_light);
  if (res.has_motion.has_value())
    this->publish_state(*res.has_motion);
}

}  // namespace esphome::xiaomi_mjyd02yla
