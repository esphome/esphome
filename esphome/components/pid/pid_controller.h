#pragma once

#include "esphome/core/esphal.h"

namespace esphome {
namespace pid {

struct PIDController {
  float update(float setpoint, float process_value) {
    // e(t) ... error at timestamp t
    // r(t) ... setpoint
    // y(t) ... process value (sensor reading)
    // u(t) ... output value

    // e(t) := r(t) - y(t)
    error = setpoint - process_value;

    // p(t) := K_p * e(t)
    proportional_term = kp * error;

    // i(t) := K_i * \int_{0}^{t} e(t) dt
    accumulated_integral_ += error * sample_time * ki;
    // constrain accumulated integral value
    if (!isnan(min_integral) && accumulated_integral_ < min_integral)
      accumulated_integral_ = min_integral;
    if (!isnan(max_integral) && accumulated_integral_ > max_integral)
      accumulated_integral_ = max_integral;
    integral_term = accumulated_integral_;

    // d(t) := K_d * de(t)/dt
    float derivative = (error - previous_error_) / sample_time;
    derivative_term = kd * derivative;
    previous_error_ = error;

    // u(t) := p(t) + i(t) + d(t)
    return proportional_term + integral_term + derivative_term;
  }

  /// Proportional gain K_p.
  float kp = 0;
  /// Integral gain K_i.
  float ki = 0;
  /// Differential gain K_d.
  float kd = 0;

  float min_integral = NAN;
  float max_integral = NAN;

  /// The time between measurements in seconds.
  float sample_time;

  // Store computed values in struct so that values can be monitored through sensors
  float error;
  float proportional_term;
  float integral_term;
  float derivative_term;

 protected:
  /// Error from previous update used for derivative term
  float previous_error_ = 0;
  /// Accumulated integral value
  float accumulated_integral_ = 0;
};

}  // namespace pid
}  // namespace esphome
