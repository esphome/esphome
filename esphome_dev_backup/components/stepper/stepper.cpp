#include "stepper.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace stepper {

static const char *const TAG = "stepper";

void Stepper::calculate_speed_(uint32_t now) {
  // delta t since last calculation in seconds
  float dt = (now - this->last_calculation_) * 1e-6f;
  this->last_calculation_ = now;
  if (this->has_reached_target()) {
    this->current_speed_ = 0.0f;
    return;
  }

  int32_t num_steps = abs(int32_t(this->target_position) - int32_t(this->current_position));
  // (v_0)^2 / 2*a
  float v_squared = this->current_speed_ * this->current_speed_;
  auto steps_to_decelerate = static_cast<int32_t>(v_squared / (2 * this->deceleration_));
  if (num_steps <= steps_to_decelerate) {
    // need to start decelerating
    this->current_speed_ -= this->deceleration_ * dt;
  } else {
    // we can still accelerate
    this->current_speed_ += this->acceleration_ * dt;
  }
  this->current_speed_ = clamp(this->current_speed_, 0.0f, this->max_speed_);
}
int32_t Stepper::should_step_() {
  uint32_t now = micros();
  this->calculate_speed_(now);
  if (this->current_speed_ == 0.0f)
    return 0;

  // assumes this method is called in a constant interval
  uint32_t dt = now - this->last_step_;
  if (dt >= (1 / this->current_speed_) * 1e6f) {
    int32_t mag = this->target_position > this->current_position ? 1 : -1;
    this->last_step_ = now;
    this->current_position += mag;
    return mag;
  }

  return 0;
}

}  // namespace stepper
}  // namespace esphome
