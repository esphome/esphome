#include "m5pm1_float_output.h"
#include <cstdint>
#include "esphome/core/progmem.h"

namespace esphome::m5pm1 {

static const char *TAG = "m5pm1.output";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG

PROGMEM_STRING_TABLE(PWMChannelStrings, "PWM_1", "PWM_2", "Unknown");

PROGMEM_STRING_TABLE(PWMPolarityStrings, "ACTIVE_HIGH", "ACTIVE_LOW");

static const LogString *pwm_channel_to_string(M5PM1PWMChannel channel) {
  return PWMChannelStrings::get_log_str(static_cast<uint8_t>(channel), PWMChannelStrings::LAST_INDEX);
}

static const LogString *pwm_polarity_to_string(M5PM1PWMPolarity polar) {
  return PWMPolarityStrings::get_log_str(static_cast<uint8_t>(polar), 0);
}

#endif

void M5PM1FloatOutput::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "PWM pin is not configured");
    this->mark_failed();
    return;
  }

  // configure the pin as output
  this->pin_->setup();

  // declare the pin as alternate function (PWM)
  if (!this->parent_->set_pin_function(this->pin_->get_pin(), M5PM1PinFunction::M5PM1_PIN_FUNCTION_ALTERNATE)) {
    ESP_LOGE(TAG, "Failed to configure PWM GPIO function");
    this->mark_failed();
  }
}

void M5PM1FloatOutput::write_state(float state) {
  // Convert the float value to uint16_t register values (12 bit)
  const uint16_t duty = static_cast<uint16_t>(state * 0x0FFF);
  this->parent_->set_pwm(this->channel_, duty, this->polar_, true);
}

void M5PM1FloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5IOE1 Float Output:\n"
                "  PWM Channel: %s\n"
                "  PWM Polarity: %s",
                LOG_STR_ARG(pwm_channel_to_string(this->channel_)), LOG_STR_ARG(pwm_polarity_to_string(this->polar_)));
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace esphome::m5pm1
