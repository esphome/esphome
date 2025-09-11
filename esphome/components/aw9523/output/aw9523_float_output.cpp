#include "aw9523_float_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace aw9523 {

static const char *const TAG = "aw9523_float_channel";

void AW9523FloatOutputChannel::write_state(float state) {
  const float duty_rounded = roundf(state * 0xff);
  auto duty = static_cast<uint8_t>(duty_rounded);
  this->parent_->set_pin_value(this->pin_, duty);
}

void AW9523FloatOutputChannel::setup() {
  if (this->max_current_ > this->parent_->get_max_current()) {
    ESP_LOGW(TAG, "Configured current %.2fmA exceeds global max current %.2fmA", this->max_current_,
             this->parent_->get_max_current());
    this->max_current_ = this->parent_->get_max_current();
  }
  this->max_power_ = this->max_current_ / this->parent_->get_max_current();
  this->parent_->led_driver(this->pin_);
  this->turn_off();
}

void AW9523FloatOutputChannel::dump_config() {
  ESP_LOGCONFIG(TAG,
                "AW9523 PWM:\n"
                "  Pin: %d\n"
                "  Max current: %.2f mA",
                this->pin_, this->max_current_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace aw9523
}  // namespace esphome
