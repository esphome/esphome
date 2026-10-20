#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_EQUITHERM_HEAT_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#include "pid_controller.h"

namespace esphome::number {
class Number;
}  // namespace esphome::number

namespace esphome::equitherm {

/// Weather compensation: the flow temperature follows the outdoor temperature along a heating curve, and a
/// PID on the room temperature trims it.
class EquithermClimate : public climate::Climate, public Component {
 public:
  EquithermClimate(sensor::Sensor *outdoor_sensor, sensor::Sensor *indoor_sensor)
      : outdoor_sensor_(outdoor_sensor), indoor_sensor_(indoor_sensor) {}
  void setup() override;
  void dump_config() override;

  /// Flow temperature in °C, for example an OpenTherm setpoint number
  void set_flow_setpoint_output(number::Number *number) { this->flow_setpoint_output_ = number; }
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  /// Flow temperature as a share of the configured range
  void set_heat_output(output::FloatOutput *output) { this->heat_output_ = output; }
#endif
  /// Enables the "Manual" preset, which sends this number's value instead of the curve
  void set_manual_flow_temp(number::Number *number) {
    this->manual_flow_temp_ = number;
    this->set_supported_custom_presets({"Manual"});
  }
  /// A restored state replaces this in setup()
  void set_default_target_temperature(float temp) { this->target_temperature = temp; }

  // Heating curve: t_flow = t_target + shift + hc * (t_target - t_outdoor)^(1/n)
  void set_heat_curve_coefficient(float hc) { this->hc_ = hc; }
  void set_heat_curve_exponent(float n) { this->n_ = n; }
  void set_heat_curve_shift(float shift) { this->shift_ = shift; }
  void set_min_flow_temp(float temp) { this->min_flow_temp_ = temp; }
  void set_max_flow_temp(float temp) { this->max_flow_temp_ = temp; }
  void set_write_deadband(float deadband) { this->write_deadband_ = deadband; }

  void set_kp(float kp) { this->pid_controller_.kp_ = kp; }
  void set_ki(float ki) { this->pid_controller_.ki_ = ki; }
  void set_kd(float kd) { this->pid_controller_.kd_ = kd; }
  void set_min_integral(float min_integral) { this->pid_controller_.min_integral_ = min_integral; }
  void set_max_integral(float max_integral) { this->pid_controller_.max_integral_ = max_integral; }
  void set_derivative_samples(uint8_t samples) {
    if (samples > 1)
      this->pid_controller_.derivative_window_.init(samples);
  }
  void set_threshold_low(float threshold) { this->pid_controller_.threshold_low_ = threshold; }
  void set_threshold_high(float threshold) { this->pid_controller_.threshold_high_ = threshold; }
  void set_kp_multiplier(float mult) { this->pid_controller_.kp_multiplier_ = mult; }
  void set_ki_multiplier(float mult) { this->pid_controller_.ki_multiplier_ = mult; }
  void set_kd_multiplier(float mult) { this->pid_controller_.kd_multiplier_ = mult; }

  template<typename F> void add_on_heating_start_callback(F &&callback) {
    this->on_heating_start_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_heating_stop_callback(F &&callback) {
    this->on_heating_stop_callback_.add(std::forward<F>(callback));
  }

  /// Recompute the flow temperature now; with update_pid the PID takes a step as well
  void force_recalculate(bool update_pid) { this->compute_and_apply_(update_pid); }

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  void compute_and_apply_(bool update_pid = true);
  /// The curve's flow temperature, within the flow limits; only meaningful while the target is above outdoors
  float curve_flow_temperature_(float t_target, float t_outdoor) const;
  /// Fires the heating triggers and turns the output off when heating ends
  void set_action_(climate::ClimateAction action);
  void write_setpoint_(float temp_c);
  void write_setpoint_off_();

  sensor::Sensor *outdoor_sensor_;
  sensor::Sensor *indoor_sensor_;
  number::Number *flow_setpoint_output_{nullptr};
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  output::FloatOutput *heat_output_{nullptr};
#endif
  number::Number *manual_flow_temp_{nullptr};

  PIDController pid_controller_;
  float hc_{1.0f};
  float n_{1.25f};
  float shift_{0.0f};
  float min_flow_temp_{25.0f};
  float max_flow_temp_{70.0f};
  /// Smallest change that is written to the output
  float write_deadband_{0.05f};
  /// Last value written to the output; NAN once heating stopped, so the next value is always written
  float active_setpoint_{NAN};
  /// Kept across recalculations that skip the PID step
  float pid_correction_{0.0f};

  LazyCallbackManager<void()> on_heating_start_callback_;
  LazyCallbackManager<void()> on_heating_stop_callback_;
};

}  // namespace esphome::equitherm
