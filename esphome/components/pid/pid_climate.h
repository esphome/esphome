#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace pid {

struct PIDController {
  double update(double setpoint, double process_value) {
    // e(t) ... error at timestamp t
    // r(t) ... setpoint
    // y(t) ... process value (sensor reading)
    // u(t) ... output value

    // e(t) := r(t) - y(t)
    double error = setpoint - process_value;

    // p(t) := K_p * e(t)
    double proportional_term = this->kp * error;

    // i(t) := K_i * \int_{0}^{t} e(t) dt
    this->accumulated_integral_ += error * this->sample_time;
    double integral_term = this->ki * error;

    // d(t) := K_d * de(t)/dt
    double derivative = (error - this->previous_error_) / this->sample_time;
    double derivative_term = this->kd * derivative;
    this->previous_error_ = error;

    // u(t) := p(t) + i(t) + d(t)
    return proportional_term + integral_term + derivative_term;
  }

  /// Proportional gain K_p.
  double kp = 0;
  /// Integral gain K_i.
  double ki = 0;
  /// Differential gain K_d.
  double kd = 0;

  /// The time between measurements in seconds.
  double sample_time;

 protected:
  /// Error from previous update used for derivative term
  double previous_error_ = 0;
  /// Accumulated integral value
  double accumulated_integral_ = 0;
};

class PIDClimate : public climate::Climate, public PollingComponent {
 public:
  PIDClimate() = default;
  void setup() override;
  void dump_config() override;

  void set_sensor(sensor::Sensor *sensor) { sensor_ = sensor; }
  void set_cool_output(output::FloatOutput *cool_output) { cool_output_ = cool_output; }
  void set_heat_output(output::FloatOutput *heat_output) { heat_output_ = heat_output; }
  void set_kp(double kp) { controller_.kp = kp; }
  void set_ki(double ki) { controller_.ki = ki; }
  void set_kd(double kd) { controller_.kd = kd; }

  float get_output_value() const { return output_value_; }

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  void update() override;

  void write_output_(float value);
  void handle_non_auto_mode_();

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_;
  output::FloatOutput *cool_output_ = nullptr;
  output::FloatOutput *heat_output_ = nullptr;
  PIDController controller_;
  /// Output value as reported by the PID controller, for PIDClimateSensor
  float output_value_;
};

}  // namespace pid
}  // namespace esphome
