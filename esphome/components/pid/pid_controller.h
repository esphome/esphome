#pragma once

#include "esphome/core/hal.h"
#include <deque>

namespace esphome {
namespace pid {

struct PIDController {
  float update(float setpoint, float process_value) {
    // e(t) ... error at timestamp t
    // r(t) ... setpoint
    // y(t) ... process value (sensor reading)
    // u(t) ... output value

    dt = calculate_relative_time_();

    // e(t) := r(t) - y(t)
    error = setpoint - process_value;

    calculate_proportional_term_();
    calculate_integral_term_();
    calculate_derivative_term_();

    // u(t) := p(t) + i(t) + d(t)
    float output = proportional_term + integral_term + derivative_term;

    // smooth/sample the output
    int samples = in_deadband() ? deadband_output_samples : output_samples;
    return weighted_average_(output_list_, output, samples);
  }

  void reset_accumulated_integral() { accumulated_integral_ = 0; }
  void set_starting_integral_term(float in) { accumulated_integral_ = in; }

  bool in_deadband() {
    // return (fabs(error) < deadband_threshold);
    float err = -error;
    return ((err > 0 && err < threshold_high) || (err < 0 && err > threshold_low));
  }

  friend class PIDClimate;

 protected:
  /// Proportional gain K_p.
  float kp = 0;
  /// Integral gain K_i.
  float ki = 0;
  /// Differential gain K_d.
  float kd = 0;

  // smooth the derivative value using a weighted average over X samples
  int derivative_samples = 8;

  /// smooth the output value using a weighted average over X values
  int output_samples = 1;

  float threshold_low = 0.0f;
  float threshold_high = 0.0f;
  float kp_multiplier = 0.0f;
  float ki_multiplier = 0.0f;
  float kd_multiplier = 0.0f;
  int deadband_output_samples = 1;

  float min_integral = NAN;
  float max_integral = NAN;

  // Store computed values in struct so that values can be monitored through sensors
  float error;
  float dt;
  float proportional_term;
  float integral_term;
  float derivative_term;

  void calculate_proportional_term_() {
    // p(t) := K_p * e(t)
    proportional_term = kp * error;

    // set dead-zone to -X to +X
    if (in_deadband()) {
      // shallow the proportional_term in the deadband by the pdm
      proportional_term *= kp_multiplier;

    } else {
      // pdm_offset prevents a jump when leaving the deadband
      float threshold = (error < 0) ? threshold_high : threshold_low;
      float pdm_offset = (threshold - (kp_multiplier * threshold)) * kp;
      proportional_term += pdm_offset;
    }
  }

  void calculate_integral_term_() {
    // i(t) := K_i * \int_{0}^{t} e(t) dt
    float new_integral = error * dt * ki;

    if (in_deadband()) {
      // shallow the integral when in the deadband
      accumulated_integral_ += new_integral * ki_multiplier;
    } else {
      accumulated_integral_ += new_integral;
    }

    // constrain accumulated integral value
    if (!std::isnan(min_integral) && accumulated_integral_ < min_integral)
      accumulated_integral_ = min_integral;
    if (!std::isnan(max_integral) && accumulated_integral_ > max_integral)
      accumulated_integral_ = max_integral;

    integral_term = accumulated_integral_;
  }

  void calculate_derivative_term_() {
    // derivative_term
    // d(t) := K_d * de(t)/dt
    float derivative = 0.0f;
    if (dt != 0.0f)
      derivative = (error - previous_error_) / dt;
    previous_error_ = error;

    // smooth the derivative samples
    derivative = weighted_average_(derivative_list_, derivative, derivative_samples);

    derivative_term = kd * derivative;

    if (in_deadband()) {
      // shallow the derivative when in the deadband
      derivative_term *= kd_multiplier;
    }
  }

  float weighted_average_(std::deque<float> &list, float new_value, int samples) {
    // if only 1 sample needed, clear the list and return
    if (samples == 1) {
      list.clear();
      return new_value;
    }

    // add the new item to the list
    list.push_front(new_value);

    // keep only 'samples' readings, by popping off the back of the list
    while (list.size() > samples)
      list.pop_back();

    // calculate and return the average of all values in the list
    float sum = 0;
    for (auto &elem : list)
      sum += elem;
    return sum / list.size();
  }

  float calculate_relative_time_() {
    uint32_t now = millis();
    uint32_t dt = now - this->last_time_;
    if (last_time_ == 0) {
      last_time_ = now;
      return 0.0f;
    }
    last_time_ = now;
    return dt / 1000.0f;
  }

  /// Error from previous update used for derivative term
  float previous_error_ = 0;
  /// Accumulated integral value
  float accumulated_integral_ = 0;
  uint32_t last_time_ = 0;

  // this is a list of derivative values for smoothing.
  std::deque<float> derivative_list_;

  // this is a list of output values for smoothing.
  std::deque<float> output_list_;
};

}  // namespace pid
}  // namespace esphome
