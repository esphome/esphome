#include "xiaomi_rtcgq02lm.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_rtcgq02lm {

ESPHOME_LOG_TAG(TAG, "xiaomi_rtcgq02lm");

static constexpr uint32_t MOTION_TIMEOUT_ID = 0;
static constexpr uint32_t BUTTON_TIMEOUT_ID = 1;

void XiaomiRTCGQ02LM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi RTCGQ02LM");
  this->dump_bindkey_(TAG);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Motion", this->motion_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Light", this->light_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Button", this->button_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
#endif
}

void XiaomiRTCGQ02LM::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
#ifdef USE_BINARY_SENSOR
  if (res.has_motion.has_value() && this->motion_binary_sensor_ != nullptr) {
    this->motion_binary_sensor_->publish_state(*res.has_motion);
    this->set_timeout(MOTION_TIMEOUT_ID, this->motion_timeout_,
                      [this]() { this->motion_binary_sensor_->publish_state(false); });
  }
  if (res.is_light.has_value() && this->light_binary_sensor_ != nullptr)
    this->light_binary_sensor_->publish_state(*res.is_light);
  if (res.button_press.has_value() && this->button_binary_sensor_ != nullptr) {
    this->button_binary_sensor_->publish_state(*res.button_press);
    this->set_timeout(BUTTON_TIMEOUT_ID, this->button_timeout_,
                      [this]() { this->button_binary_sensor_->publish_state(false); });
  }
#endif
#ifdef USE_SENSOR
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
#endif
}

}  // namespace esphome::xiaomi_rtcgq02lm
