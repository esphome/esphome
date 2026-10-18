#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome::pid {

struct PIDController {
  float update(float setpoint, float process_value);

  void reset_accumulated_integral() { accumulated_integral_ = 0; }
  void set_starting_integral_term(float in) { accumulated_integral_ = in; }

  bool in_deadband();

  friend class PIDClimate;

 private:
  /// Proportional gain K_p.
  float kp_ = 0;
  /// Integral gain K_i.
  float ki_ = 0;
  /// Differential gain K_d.
  float kd_ = 0;

  // smooth the derivative value using an average over X samples
  int derivative_samples_ = 1;

  /// smooth the output value using an average over X values
  int output_samples_ = 1;

  float threshold_low_ = 0.0f;
  float threshold_high_ = 0.0f;
  float kp_multiplier_ = 0.0f;
  float ki_multiplier_ = 0.0f;
  float kd_multiplier_ = 0.0f;
  int deadband_output_samples_ = 1;

  float min_integral_ = NAN;
  float max_integral_ = NAN;

  // Store computed values in struct so that values can be monitored through sensors
  float error_;
  float dt_;
  float proportional_term_;
  float integral_term_;
  float derivative_term_;

  void calculate_proportional_term_();
  void calculate_integral_term_();
  void calculate_derivative_term_(float setpoint);

  /// Ring buffer smoothing using FixedRingBuffer (single allocation at setup)
  float ring_buffer_average_(FixedRingBuffer<float> &buf, float new_value, int max_samples);

  float calculate_relative_time_();

  /// Error from previous update used for derivative term
  float previous_error_ = 0;
  float previous_setpoint_ = NAN;
  /// Accumulated integral value
  float accumulated_integral_ = 0;
  uint32_t last_time_ = 0;

  // Ring buffer for derivative smoothing
  FixedRingBuffer<float> derivative_window_;

  // Ring buffer for output smoothing (shared between normal and deadband modes)
  FixedRingBuffer<float> output_window_;

};  // Struct PIDController
}  // namespace esphome::pid
