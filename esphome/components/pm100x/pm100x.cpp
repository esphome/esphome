#include "pm100x.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::pm100x {

static const char *const TAG = "pm100x";

static const char *model_to_string(PM100XModel model) {
  switch (model) {
    case PM100XModel::PM1003:
      return "pm1003";
    case PM100XModel::PM1006:
      return "pm1006";
    case PM100XModel::PM1006K:
      return "pm1006k";
  }
  return "unknown";
}

void PM100XComponent::setup() {
  this->start_time_ = App.get_loop_component_start_time();
  this->initial_delay_done_ = false;
}

void PM100XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PM100X Model: %s", model_to_string(this->model_));
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

float PM100XComponent::get_setup_priority() const { return setup_priority::DATA; }

float PM100XComponent::duty_to_pm25_(float duty_percent) const {
  if (duty_percent < 0.0f)
    duty_percent = 0.0f;
  if (duty_percent > 100.0f)
    duty_percent = 100.0f;

  switch (this->model_) {
    case PM100XModel::PM1006:
    case PM100XModel::PM1006K: {
      // PWM low-level ms maps to concentration (cycle 1000ms)
      float pm_2_5_concentration = (duty_percent * 10.0f) - 4.0f;
      if (pm_2_5_concentration < 0.0f)
        pm_2_5_concentration = 0.0f;
      if (pm_2_5_concentration > 992.0f)
        pm_2_5_concentration = 992.0f;
      return pm_2_5_concentration;
    }
    case PM100XModel::PM1003:
    default: {
      float pm_2_5_concentration = (duty_percent / 100.0f) * 500.0f;
      if (pm_2_5_concentration < 0.0f)
        pm_2_5_concentration = 0.0f;
      if (pm_2_5_concentration > 500.0f)
        pm_2_5_concentration = 500.0f;
      return pm_2_5_concentration;
    }
  }
}

}  // namespace esphome::pm100x
