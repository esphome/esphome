#include "vbus_time.h"

#ifdef USE_VBUS_SETTING_TIME
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.time");

void VBusSettingTime::dump_setting_config() {
  LOG_DATETIME_TIME("", "VBus Setting", this);
  ESP_LOGCONFIG(TAG, "  Setting ID: %u", this->index_);
}

void VBusSettingTime::on_setting_snapshot() {
  if (!this->controller_->has_fresh_snapshot() && this->controller_->has_recent_snapshot())
    return;
  const float value = this->controller_->setting_value(this->index_);
  if (!std::isfinite(value) || value < 0 || value > 1439) {
    if (this->has_state()) {
      this->set_has_state(false);
#ifdef USE_CONTROLLER_REGISTRY
      ControllerRegistry::notify_time_update(this);
#endif
    }
    return;
  }
  const auto minutes = static_cast<uint16_t>(value);
  if (!this->has_state() || this->hour_ != minutes / 60 || this->minute_ != minutes % 60) {
    this->hour_ = minutes / 60;
    this->minute_ = minutes % 60;
    this->second_ = 0;
    this->publish_state();
  }
}

void VBusSettingTime::control(const datetime::TimeCall &call) {
  const auto hour = call.get_hour();
  const auto minute = call.get_minute();
  const auto second = call.get_second();
  if (!hour.has_value() || !minute.has_value() || !second.has_value() || *second != 0) {
    ESP_LOGW(TAG, "Clock settings require a complete HH:MM:00 time");
    return;
  }
  const uint16_t value = uint16_t(*hour) * 60 + *minute;
  this->controller_->request_setting(this->index_, value);
}

}  // namespace esphome::vbus
#endif
