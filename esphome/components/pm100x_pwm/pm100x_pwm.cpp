#include "pm100x_pwm.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cmath>

namespace esphome::pm100x_pwm {

static const char *const TAG = "pm100x_pwm";

void PM100XComponentPWM::setup() {
  pm100x::PM100XComponent::setup();
  if (this->pwm_sensor_ != nullptr) {
    this->pwm_sensor_->add_on_state_callback([this](float duty_percent) { this->handle_pwm_state_(duty_percent); });
  }
}

void PM100XComponentPWM::dump_config() {
  pm100x::PM100XComponent::dump_config();
  LOG_SENSOR("  ", "PWM Duty Percent", this->pwm_sensor_);
}

void PM100XComponentPWM::handle_pwm_state_(float duty_percent) {
  if (!this->initial_delay_done_) {
    const uint32_t now = App.get_loop_component_start_time();
    if (now - this->start_time_ < this->startup_delay_ms_) {
      return;
    }
    this->initial_delay_done_ = true;
  }
  if (this->pm_2_5_sensor_ == nullptr)
    return;
  if (std::isnan(duty_percent) || duty_percent < 0.0f)
    return;
  float pm_2_5_concentration = this->duty_to_pm25_(duty_percent);

  ESP_LOGD(TAG, "PWM duty %.1f%% -> PM2.5 %.1f µg/m³", duty_percent, pm_2_5_concentration);
  this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
}

}  // namespace esphome::pm100x_pwm
