#ifdef USE_ZEPHYR

#include "zephyr_pwm.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"

#include <zephyr/drivers/pwm.h>
#include <cmath>

namespace esphome::zephyr_pwm {

static const char *const TAG = "zephyr_pwm";

void ZephyrPWMChannel::setup() {
  if (!device_is_ready(this->device_)) {
    ESP_LOGE(TAG, "pwm is not ready.");
    this->mark_failed();
    return;
  }
}

void ZephyrPWMChannel::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zephyr PWM:\n"
                "  Channel: %u\n"
                "  Period: %u ns",
                this->channel_, this->period_ns_);
  LOG_FLOAT_OUTPUT(this);
}
void HOT ZephyrPWMChannel::write_state(float state) {
  this->last_output_ = state;

  uint32_t pulse_width_ns = state * this->period_ns_;
  pwm_flags_t flags = this->inverted_ ? PWM_POLARITY_INVERTED : PWM_POLARITY_NORMAL;
  int err = pwm_set(this->device_, this->channel_, this->period_ns_, pulse_width_ns, flags);
  if (err != 0) {
    ESP_LOGE(TAG, "Failed to set PWM output: channel=%u, period=%u ns, pulse_width=%u ns, error=%d", this->channel_,
             this->period_ns_, pulse_width_ns, err);
  }
}

}  // namespace esphome::zephyr_pwm

#endif  // USE_ZEPHYR
