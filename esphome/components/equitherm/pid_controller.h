#pragma once

#include "esphome/core/helpers.h"

#include <cmath>

namespace esphome::equitherm {

/// The PID of the pid climate, reduced to what the room temperature correction needs.
struct PIDController {
  /// The correction for the error between setpoint and process_value since the previous call
  float update(float setpoint, float process_value);

  void reset_accumulated_integral() { this->accumulated_integral_ = 0; }

  friend class EquithermClimate;

 private:
  bool in_deadband_() const { return this->threshold_low_ < -this->error_ && -this->error_ < this->threshold_high_; }
  /// Seconds since the previous call, 0 on the first one
  float relative_time_();

  float kp_{0};
  float ki_{0};
  float kd_{0};
  float min_integral_{-1};
  float max_integral_{1};

  // Inside the deadband the terms are scaled down by these
  float threshold_low_{0};
  float threshold_high_{0};
  float kp_multiplier_{0};
  float ki_multiplier_{0};
  float kd_multiplier_{0};

  float error_{0};
  float previous_error_{0};
  float previous_setpoint_{NAN};
  float accumulated_integral_{0};
  uint32_t last_time_{0};
  /// Smooths the derivative over the configured samples; stays empty for a single sample
  FixedRingBuffer<float, 16> derivative_window_;
};

}  // namespace esphome::equitherm
