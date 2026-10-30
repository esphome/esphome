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
  sensor::publish_optional(this->idle_time_sensor_, res.idle_time);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
  sensor::publish_optional(this->illuminance_sensor_, res.illuminance);
  if (res.is_light.has_value() && this->light_binary_sensor_ != nullptr)
    this->light_binary_sensor_->publish_state(*res.is_light);
  if (res.has_motion.has_value())
    this->publish_state(*res.has_motion);
}

}  // namespace esphome::xiaomi_mjyd02yla
