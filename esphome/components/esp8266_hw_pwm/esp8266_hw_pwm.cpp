#ifdef USE_ESP8266

#include "esp8266_hw_pwm.h"
#include "esphome/core/macros.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp8266_hw_pwm {

static const char *const TAG = "esp8266_hw_pwm";

void ESP8266HWPWM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP8266 hardware PWM Output...");
}

void ESP8266HWPWM::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP8266 hardware PWM:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
  LOG_FLOAT_OUTPUT(this);
}

void HOT ESP8266HWPWM::write_state(float state) {
  this->last_output_ = state;

  // Also check pin inversion
  if (this->pin_->is_inverted()) {
    state = 1.0f - state;
  }
}

}  // namespace esp8266_hw_pwm
}  // namespace esphome

#endif
