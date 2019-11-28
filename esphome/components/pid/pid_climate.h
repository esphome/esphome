#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"
#include "pid_controller.h"
#include "pid_autotuner.h"

namespace esphome {
namespace pid {

class PIDClimate : public climate::Climate, public PollingComponent {
 public:
  PIDClimate() = default;
  void setup() override;
  void dump_config() override;

  void set_sensor(sensor::Sensor *sensor) { sensor_ = sensor; }
  void set_cool_output(output::FloatOutput *cool_output) { cool_output_ = cool_output; }
  void set_heat_output(output::FloatOutput *heat_output) { heat_output_ = heat_output; }
  void set_kp(float kp) { controller_.kp = kp; }
  void set_ki(float ki) { controller_.ki = ki; }
  void set_kd(float kd) { controller_.kd = kd; }

  float get_output_value() const { return output_value_; }
  float get_error_value() const { return controller_.error; }
  float get_proportional_term() const { return controller_.proportional_term; }
  float get_integral_term() const { return controller_.integral_term; }
  float get_derivative_term() const { return controller_.derivative_term; }
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }
  void set_default_target_temperature(float default_target_temperature) {
    default_target_temperature_ = default_target_temperature;
  }

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
  CallbackManager<void()> pid_computed_callback_;
  float default_target_temperature_;
  std::unique_ptr<PIDAutotuner> autotuner_;
};

}  // namespace pid
}  // namespace esphome
