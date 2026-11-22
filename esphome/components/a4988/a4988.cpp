#include "a4988.h"
#include "esphome/core/log.h"

namespace esphome::a4988 {

ESPHOME_LOG_TAG(TAG, "a4988.stepper");

void A4988::setup() {
  if (this->sleep_pin_ != nullptr) {
    setup_output_pin(this->sleep_pin_, false);
    this->sleep_pin_state_ = false;
  }
  setup_output_pin(this->step_pin_, false);
  setup_output_pin(this->dir_pin_, false);
}
void A4988::dump_config() {
  ESP_LOGCONFIG(TAG, "A4988:");
  LOG_PIN("  Step Pin: ", this->step_pin_);
  LOG_PIN("  Dir Pin: ", this->dir_pin_);
  LOG_PIN("  Sleep Pin: ", this->sleep_pin_);
  LOG_STEPPER(this);
}
void A4988::loop() {
  bool at_target = this->has_reached_target();
  if (this->sleep_pin_ != nullptr) {
    bool sleep_rising_edge = !sleep_pin_state_ & !at_target;
    this->sleep_pin_->digital_write(!at_target);
    this->sleep_pin_state_ = !at_target;
    if (sleep_rising_edge) {
      delayMicroseconds(1000);
    }
  }
  this->high_freq_.set(!at_target);

  int32_t dir = this->should_step_();
  if (dir == 0)
    return;

  this->dir_pin_->digital_write(dir == 1);
  delayMicroseconds(50);
  pulse_pin(this->step_pin_, true, 5);
}

}  // namespace esphome::a4988
