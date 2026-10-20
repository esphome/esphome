#include "pid_controller.h"
#include "esphome/core/hal.h"

namespace esphome::equitherm {

float PIDController::update(float setpoint, float process_value) {
  const float dt = this->relative_time_();
  this->error_ = setpoint - process_value;
  const bool deadband = this->in_deadband_();

  // p(t) := K_p * e(t); outside the deadband an offset keeps the term continuous at its edge
  float proportional = this->kp_ * this->error_;
  if (deadband) {
    proportional *= this->kp_multiplier_;
  } else {
    const float threshold = this->error_ < 0 ? this->threshold_high_ : this->threshold_low_;
    proportional += (threshold - this->kp_multiplier_ * threshold) * this->kp_;
  }

  // i(t) := K_i * integral of e(t) dt
  this->accumulated_integral_ += this->error_ * dt * this->ki_ * (deadband ? this->ki_multiplier_ : 1.0f);
  this->accumulated_integral_ = clamp(this->accumulated_integral_, this->min_integral_, this->max_integral_);

  // d(t) := K_d * de(t)/dt, with a setpoint change taken out of the error
  float derivative = 0.0f;
  if (dt != 0.0f) {
    if (!std::isnan(this->previous_setpoint_) && this->previous_setpoint_ != setpoint)
      this->previous_error_ -= this->previous_setpoint_ - setpoint;
    derivative = (this->error_ - this->previous_error_) / dt;
  }
  this->previous_error_ = this->error_;
  this->previous_setpoint_ = setpoint;
  if (this->derivative_window_.capacity() > 1) {
    if (this->derivative_window_.full())
      this->derivative_window_.pop();
    this->derivative_window_.push(derivative);
    float sum = 0;
    for (float value : this->derivative_window_)
      sum += value;
    derivative = sum / this->derivative_window_.size();
  }
  const float derivative_term = this->kd_ * derivative * (deadband ? this->kd_multiplier_ : 1.0f);

  return proportional + this->accumulated_integral_ + derivative_term;
}

float PIDController::relative_time_() {
  const uint32_t now = millis();
  const uint32_t dt = now - this->last_time_;
  const bool first = this->last_time_ == 0;
  this->last_time_ = now;
  return first ? 0.0f : dt / 1000.0f;
}

}  // namespace esphome::equitherm
