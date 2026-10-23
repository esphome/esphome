#include "xiaomi_cgpr1.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgpr1 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgpr1");

void XiaomiCGPR1::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGPR1");
  LOG_BINARY_SENSOR("  ", "Motion", this);
  LOG_SENSOR("  ", "Idle Time", this->idle_time_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
}

void XiaomiCGPR1::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.idle_time.has_value() && this->idle_time_ != nullptr)
    this->idle_time_->publish_state(*res.idle_time);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
  if (res.illuminance.has_value() && this->illuminance_ != nullptr)
    this->illuminance_->publish_state(*res.illuminance);
  if (res.has_motion.has_value())
    this->publish_state(*res.has_motion);
}

}  // namespace esphome::xiaomi_cgpr1
