#include "xiaomi_cgpr1.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgpr1 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgpr1");

void XiaomiCGPR1::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGPR1");
  this->dump_bindkey_(TAG);
  LOG_BINARY_SENSOR("  ", "Motion", this);
  LOG_SENSOR("  ", "Idle Time", this->idle_time_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
}

void XiaomiCGPR1::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  publish_optional(this->idle_time_sensor_, res.idle_time);
  publish_optional(this->battery_level_sensor_, res.battery_level);
  publish_optional(this->illuminance_sensor_, res.illuminance);
  if (res.has_motion.has_value())
    this->publish_state(*res.has_motion);
}

}  // namespace esphome::xiaomi_cgpr1
