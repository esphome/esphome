#include "pid_controller.h"

namespace esphome::pid {

float PIDController::update(float setpoint, float process_value) {
  // e(t) ... error at timestamp t
  // r(t) ... setpoint
  // y(t) ... process value (sensor reading)
  // u(t) ... output value

  dt_ = calculate_relative_time_();

  // e(t) := r(t) - y(t)
  error_ = setpoint - process_value;

  calculate_proportional_term_();
  calculate_integral_term_();
  calculate_derivative_term_(setpoint);

  // u(t) := p(t) + i(t) + d(t)
  float output = proportional_term_ + integral_term_ + derivative_term_;

  // smooth/sample the output using shared buffer with mode-appropriate sample count
  int samples = in_deadband() ? deadband_output_samples_ : output_samples_;
  return ring_buffer_average_(output_window_, output, samples);
}

bool PIDController::in_deadband() {
  // return (fabs(error) < deadband_threshold);
  float err = -error_;
  return (threshold_low_ < err && err < threshold_high_);
}

void PIDController::calculate_proportional_term_() {
  // p(t) := K_p * e(t)
  proportional_term_ = kp_ * error_;

  // set dead-zone to -X to +X
  if (in_deadband()) {
    // shallow the proportional_term in the deadband by the pdm
    proportional_term_ *= kp_multiplier_;

  } else {
    // pdm_offset prevents a jump when leaving the deadband
    float threshold = (error_ < 0) ? threshold_high_ : threshold_low_;
    float pdm_offset = (threshold - (kp_multiplier_ * threshold)) * kp_;
    proportional_term_ += pdm_offset;
  }
}

void PIDController::calculate_integral_term_() {
  // i(t) := K_i * \int_{0}^{t} e(t) dt
  float new_integral = error_ * dt_ * ki_;

  if (in_deadband()) {
    // shallow the integral when in the deadband
    accumulated_integral_ += new_integral * ki_multiplier_;
  } else {
    accumulated_integral_ += new_integral;
  }

  // constrain accumulated integral value
  if (!std::isnan(min_integral_) && accumulated_integral_ < min_integral_)
    accumulated_integral_ = min_integral_;
  if (!std::isnan(max_integral_) && accumulated_integral_ > max_integral_)
    accumulated_integral_ = max_integral_;

  integral_term_ = accumulated_integral_;
}

void PIDController::calculate_derivative_term_(float setpoint) {
  // derivative_term_
  // d(t) := K_d * de(t)/dt
  float derivative = 0.0f;
  if (dt_ != 0.0f) {
    // remove changes to setpoint from error
    if (!std::isnan(previous_setpoint_) && previous_setpoint_ != setpoint)
      previous_error_ -= previous_setpoint_ - setpoint;
    derivative = (error_ - previous_error_) / dt_;
  }
  previous_error_ = error_;
  previous_setpoint_ = setpoint;

  // smooth the derivative samples
  derivative = ring_buffer_average_(derivative_window_, derivative, derivative_samples_);

  derivative_term_ = kd_ * derivative;

  if (in_deadband()) {
    // shallow the derivative when in the deadband
    derivative_term_ *= kd_multiplier_;
  }
}

float PIDController::ring_buffer_average_(FixedRingBuffer<float> &buf, float new_value, int max_samples) {
  // if only 1 sample needed (or invalid), clear the buffer and return
  if (max_samples <= 1) {
    buf.clear();
    return new_value;
  }

  // Trim oldest entries to make room (handles mode-switching where buffer
  // may have more entries than the current mode needs)
  while (buf.size() >= static_cast<size_t>(max_samples))
    buf.pop();
  buf.push(new_value);

  float sum = 0;
  for (auto val : buf)
    sum += val;
  return sum / buf.size();
}

float PIDController::calculate_relative_time_() {
  uint32_t now = millis();
  uint32_t dt = now - this->last_time_;
  if (last_time_ == 0) {
    last_time_ = now;
    return 0.0f;
  }
  last_time_ = now;
  return dt / 1000.0f;
}

}  // namespace esphome::pid
