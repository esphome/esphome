#pragma once

#include <functional>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_EQUITHERM_HEAT_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#include "equitherm_controller.h"
#include "pid_controller.h"

namespace esphome::number {
class Number;  // Forward declaration
}  // namespace esphome::number

namespace esphome::equitherm {

class EquithermClimate : public climate::Climate, public Component {
 public:
  EquithermClimate() = default;
  void setup() override;
  void dump_config() override;

  // Sensor inputs
  void set_outdoor_sensor(sensor::Sensor *sensor) { outdoor_sensor_ = sensor; }
  void set_indoor_sensor(sensor::Sensor *sensor) { indoor_sensor_ = sensor; }

  // Output (mutually exclusive - only one should be set)
  void set_flow_setpoint_output(number::Number *number) { flow_setpoint_output_ = number; }
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  void set_heat_output(output::FloatOutput *output) { heat_output_ = output; }
#endif

  // Optional manual flow temperature (used when manual preset is active)
  void set_manual_flow_temp(number::Number *number) { manual_flow_temp_ = number; }

  // Climate defaults
  void set_default_target_temperature(float temp) { default_target_temperature_ = temp; }

  // Heating curve parameters (delegated to heating curve)
  void set_heat_curve_coefficient(float hc) { heating_curve_.set_hc(hc); }
  void set_heat_curve_exponent(float n) { heating_curve_.set_n(n); }
  void set_heat_curve_shift(float shift) { heating_curve_.set_shift(shift); }

  // Output parameters
  void set_min_flow_temp(float temp) { heating_curve_.set_min_flow_temp(temp); }
  void set_max_flow_temp(float temp) { heating_curve_.set_max_flow_temp(temp); }
  void set_write_deadband(float deadband) { write_deadband_ = deadband; }

  // PID parameters
  void set_kp(float kp) { pid_controller_.kp_ = kp; }
  void set_ki(float ki) { pid_controller_.ki_ = ki; }
  void set_kd(float kd) { pid_controller_.kd_ = kd; }
  void set_min_integral(float min_integral) { pid_controller_.min_integral_ = min_integral; }
  void set_max_integral(float max_integral) { pid_controller_.max_integral_ = max_integral; }
  void set_derivative_samples(int samples) {
    pid_controller_.derivative_samples_ = samples;
    if (samples > 1)  // No allocation needed when samples=1 (ring_buffer_average_ short-circuits)
      pid_controller_.derivative_window_.init(samples);
  }

  // Deadband parameters
  void set_threshold_low(float threshold) { pid_controller_.threshold_low_ = threshold; }
  void set_threshold_high(float threshold) { pid_controller_.threshold_high_ = threshold; }
  void set_kp_multiplier(float mult) { pid_controller_.kp_multiplier_ = mult; }
  void set_ki_multiplier(float mult) { pid_controller_.ki_multiplier_ = mult; }
  void set_kd_multiplier(float mult) { pid_controller_.kd_multiplier_ = mult; }

  bool is_wws_active() const { return wws_active_; }

  // State getters (for diagnostics)
  float get_heating_curve_output() const { return heating_curve_output_; }
  float get_pid_adjusted_output() const { return pid_adjusted_output_; }
  float get_flow_setpoint() const { return flow_setpoint_; }
  float get_active_setpoint() const { return active_setpoint_; }
  float get_pid_correction() const { return pid_correction_; }
  bool in_deadband() { return pid_controller_.in_deadband(); }
  bool is_pid_active() const {
    // Check if any PID gain is effectively non-zero (handles runtime tuning)
    constexpr float epsilon = 0.0001f;
    return fabsf(pid_controller_.kp_) > epsilon || fabsf(pid_controller_.ki_) > epsilon ||
           fabsf(pid_controller_.kd_) > epsilon;
  }

  // Parameter getters (for runtime tuning)
  float get_hc() const { return heating_curve_.get_hc(); }
  float get_n() const { return heating_curve_.get_n(); }
  float get_shift() const { return heating_curve_.get_shift(); }
  float get_min_flow_temp() const { return heating_curve_.get_min_flow_temp(); }
  float get_max_flow_temp() const { return heating_curve_.get_max_flow_temp(); }
  float get_kp() const { return pid_controller_.kp_; }
  float get_ki() const { return pid_controller_.ki_; }
  float get_kd() const { return pid_controller_.kd_; }
  float get_proportional_term() const { return pid_controller_.proportional_term_; }
  float get_integral_term() const { return pid_controller_.integral_term_; }
  float get_derivative_term() const { return pid_controller_.derivative_term_; }

  // Callback for diagnostic sensors
  void add_on_state_callback(std::function<void()> &&callback) { state_callback_.add(std::move(callback)); }

  // Callbacks for heating state transitions
  template<typename F> void add_on_heating_start_callback(F &&callback) {
    this->on_heating_start_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_heating_stop_callback(F &&callback) {
    this->on_heating_stop_callback_.add(std::forward<F>(callback));
  }

  // Force immediate recalculation (used by runtime tuning numbers)
  void force_recalculate(bool update_pid = false) { this->compute_and_apply_(update_pid); }

 protected:
  /// Override control to handle climate calls from HA
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller
  climate::ClimateTraits traits() override;

  void compute_and_apply_(bool update_pid = true);
  void write_setpoint_(float temp_c);
  void write_setpoint_off_();

  /// Outdoor temperature sensor (required)
  sensor::Sensor *outdoor_sensor_{nullptr};
  /// Indoor temperature sensor (required - for current_temperature display and room correction)
  sensor::Sensor *indoor_sensor_{nullptr};
  /// Flow temperature setpoint output (direct °C control, e.g., OpenTherm)
  number::Number *flow_setpoint_output_{nullptr};
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  /// Alternative: generic float output (normalized 0-1)
  output::FloatOutput *heat_output_{nullptr};
#endif
  /// Manual flow temperature override (used when custom preset "manual" is active)
  number::Number *manual_flow_temp_{nullptr};

  /// Curve calculation engine
  HeatingCurve heating_curve_;
  /// PID controller for fine-tuning equitherm output
  PIDController pid_controller_;
  /// Default target temperature when no state restored
  float default_target_temperature_{20.0f};
  /// Raw heating curve output before PID (for diagnostics)
  float heating_curve_output_{NAN};
  /// Setpoint after PID (for diagnostics)
  float pid_adjusted_output_{NAN};
  /// Whether warm weather shutdown is active (delta_t <= 0, no heating demand)
  bool wws_active_{false};
  /// Flow setpoint (after PID, for diagnostics)
  float flow_setpoint_{NAN};
  /// Last value actually written to boiler (confirmed active setpoint)
  float active_setpoint_{NAN};
  /// PID correction (for diagnostics)
  float pid_correction_{NAN};
  /// Minimum setpoint change (°C) required to write to boiler output
  float write_deadband_{0.05f};

  /// Callback for diagnostic sensors
  CallbackManager<void()> state_callback_;
  /// Previous climate action — used to detect heating start/stop transitions
  climate::ClimateAction prev_action_{climate::CLIMATE_ACTION_OFF};
  /// Callback fired when transitioning into CLIMATE_ACTION_HEATING
  CallbackManager<void()> on_heating_start_callback_;
  /// Callback fired when transitioning out of CLIMATE_ACTION_HEATING
  CallbackManager<void()> on_heating_stop_callback_;
};

template<typename... Ts> class EquithermForceRecalculateAction : public Action<Ts...> {
 public:
  EquithermForceRecalculateAction(EquithermClimate *parent) : parent_(parent) {}

  void set_update_pid(bool update_pid) { update_pid_ = update_pid; }

  void play(const Ts &...x) override { this->parent_->force_recalculate(this->update_pid_); }

 protected:
  EquithermClimate *parent_;
  bool update_pid_{true};
};

}  // namespace esphome::equitherm
