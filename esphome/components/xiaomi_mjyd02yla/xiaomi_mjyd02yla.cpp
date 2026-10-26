#include "xiaomi_mjyd02yla.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mjyd02yla {

ESPHOME_LOG_TAG(TAG, "xiaomi_mjyd02yla");

void XiaomiMJYD02YLA::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MJYD02YL-A");
  this->dump_bindkey_(TAG);
  LOG_BINARY_SENSOR("  ", "Motion", this);
  LOG_BINARY_SENSOR("  ", "Light", this->is_light_);
  LOG_SENSOR("  ", "Idle Time", this->idle_time_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
}

void XiaomiMJYD02YLA::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.idle_time.has_value() && this->idle_time_ != nullptr)
    this->idle_time_->publish_state(*res.idle_time);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
  if (res.illuminance.has_value() && this->illuminance_ != nullptr)
    this->illuminance_->publish_state(*res.illuminance);
  if (res.is_light.has_value() && this->is_light_ != nullptr)
    this->is_light_->publish_state(*res.is_light);
  if (res.has_motion.has_value())
    this->publish_state(*res.has_motion);
}

}  // namespace esphome::xiaomi_mjyd02yla
