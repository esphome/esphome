#include "m5ioe1_float_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/progmem.h"

namespace esphome::m5ioe1 {

static const char *TAG = "m5ioe1.output";

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG

PROGMEM_STRING_TABLE(PWMChannelStrings, "PWM_1", "PWM_2", "PWM_3", "PWM_4", "Unknown");

PROGMEM_STRING_TABLE(PWMPolarityStrings, "ACTIVE_HIGH", "ACTIVE_LOW");

static const LogString *pwm_channel_to_string(M5IOE1PWMChannel channel) {
  return PWMChannelStrings::get_log_str(static_cast<uint8_t>(channel), PWMChannelStrings::LAST_INDEX);
}

static const LogString *pwm_polarity_to_string(M5IOE1PWMPolarity polar) {
  return PWMPolarityStrings::get_log_str(static_cast<uint8_t>(polar), 0);
}

#endif

void M5IOE1FloatOutput::setup() { this->parent_->setup_pwm_channel(this->channel_, this->polar_); }

void M5IOE1FloatOutput::write_state(float state) { this->parent_->set_pwm_value(this->channel_, state); }

void M5IOE1FloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5IOE1 Float Output:\n"
                "  PWM Channel: %s\n"
                "  PWM Polarity: %s",
                LOG_STR_ARG(pwm_channel_to_string(this->channel_)), LOG_STR_ARG(pwm_polarity_to_string(this->polar_)));
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace esphome::m5ioe1
