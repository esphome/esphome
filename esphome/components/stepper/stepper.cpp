#include "stepper.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace stepper {

static const char *const TAG = "stepper";

/**
 * @brief Decelerates to standstill.
 *
 */
void Stepper::request_stop() {
  if (this->current_speed_ == 0.0f) {
    this->target_position = this->current_position;

  } else {
    // Compute steps needed to decelerate:
    float v_squared = this->current_speed_ * this->current_speed_;
    auto steps_to_decelerate = static_cast<int32_t>(v_squared / (2 * this->deceleration_));

    // Adjust the target:

    int32_t target_dist = abs(int32_t(this->target_position) - int32_t(this->current_position));
    if (target_dist <= steps_to_decelerate) {
      // Probably already decelerating, so let's not increase the distance
      steps_to_decelerate = target_dist;
    }

    // Based on the direction of movement, adjust the target position:
    if (this->current_speed_ < 0) {
      this->target_position = this->current_position - steps_to_decelerate;
    } else
      this->target_position = this->current_position + steps_to_decelerate;
  }
}

void Stepper::calculate_speed_(uint32_t now) {
  // delta t since last calculation in seconds
  float dt = (now - this->last_calculation_) * 1e-6f;
  this->last_calculation_ = now;
  if (this->has_reached_target()) {
    this->current_speed_ = 0.0f;
    return;
  }

  int32_t target_difference = int32_t(this->target_position) - int32_t(this->current_position);

  if ((target_difference > 0) && (this->current_speed_ < 0)) {
    // wrong direction (negative), need to decelerate:
    this->current_speed_ += this->deceleration_ * dt;
    return;
  }

  if ((target_difference < 0) && (this->current_speed_ > 0)) {
    // wrong direction (positive), need to decelerate:
    this->current_speed_ -= this->deceleration_ * dt;
    return;
  }

  int32_t num_steps = abs(int32_t(this->target_position) - int32_t(this->current_position));
  // (v_0)^2 / 2*a
  float v_squared = this->current_speed_ * this->current_speed_;
  auto steps_to_decelerate = static_cast<int32_t>(v_squared / (2 * this->deceleration_));
  if (num_steps <= steps_to_decelerate) {
    // need to start decelerating
    if (target_difference > 0) {
      this->current_speed_ -= this->deceleration_ * dt;
    } else {
      this->current_speed_ += this->deceleration_ * dt;
    }
  } else {
    // we can still accelerate
    if (target_difference > 0) {
      this->current_speed_ += this->acceleration_ * dt;
    } else {
      this->current_speed_ -= this->acceleration_ * dt;
    }
  }

  this->current_speed_ = clamp(this->current_speed_, -this->max_speed_, this->max_speed_);
}

int32_t Stepper::should_step_() {
  uint32_t now = micros();
  this->calculate_speed_(now);
  if (this->current_speed_ == 0.0f)
    return 0;

  // assumes this method is called in a constant interval
  uint32_t dt = now - this->last_step_;
  if (dt >= (1. / std::abs(this->current_speed_)) * 1e6f) {
    int32_t mag = this->current_speed_ > 0 ? 1 : -1;
    this->last_step_ = now;
    this->current_position += mag;
    return mag;
  }

  return 0;
}

}  // namespace stepper
}  // namespace esphome
