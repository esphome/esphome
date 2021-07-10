#include "a4988.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a4988 {

static const char *const TAG = "a4988.stepper";

void A4988::setup() {
  ESP_LOGCONFIG(TAG, "Setting up A4988...");
  if (this->sleep_pin_ != nullptr) {
    this->sleep_pin_->setup();
    this->sleep_pin_->digital_write(false);
    this->sleep_pin_state_ = false;
  }
  this->step_pin_->setup();
  this->step_pin_->digital_write(false);
  this->dir_pin_->setup();
  this->dir_pin_->digital_write(false);
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
  if (at_target) {
    this->high_freq_.stop();
  } else {
    this->high_freq_.start();
  }

  int32_t dir = this->should_step_();
  if (dir == 0)
    return;

  this->dir_pin_->digital_write(dir == 1);
  this->step_pin_->digital_write(true);
  delayMicroseconds(5);
  this->step_pin_->digital_write(false);
}

}  // namespace a4988
}  // namespace esphome
