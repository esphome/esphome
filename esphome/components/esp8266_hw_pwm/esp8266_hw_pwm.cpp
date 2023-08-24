#ifdef USE_ESP8266

#include "esp8266_hw_pwm.h"
#include "esphome/core/log.h"

#include "pwm_i2s.h"

namespace esphome {
namespace esp8266_hw_pwm {

static const char *const TAG = "esp8266_hw_pwm";

void ESP8266HWPWM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP8266 hardware PWM Output...");
  pwm_begin(this->frequency_);
  this->_pwm_debug();
}

void ESP8266HWPWM::_pwm_debug() {
  float actual_rate = pwm_get_rate();
  ESP_LOGD(TAG, "Effective PWM frequency: %fHz (%f%% as requested)", actual_rate, 100*actual_rate/this->frequency_);
  ESP_LOGD(TAG, "Available PWM levels: %d", pwm_levels());
}

void ESP8266HWPWM::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP8266 hardware PWM:");
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
  LOG_FLOAT_OUTPUT(this);
}

void HOT ESP8266HWPWM::write_state(float state) {
  this->last_output_ = state;

  if (this->frequency_changed_) {
    pwm_set_rate(this->frequency_);
    this->frequency_changed_ = false;
    this->_pwm_debug();
  }

  pwm_set_level(state);
}

}  // namespace esp8266_hw_pwm
}  // namespace esphome

#endif  // USE_ESP8266
