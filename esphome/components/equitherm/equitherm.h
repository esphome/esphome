#pragma once

#include <functional>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"
#include "equitherm_controller.h"
#include "pid_controller.h"

namespace esphome {
namespace number {
class Number;  // Forward declaration
}
namespace equitherm {

class EquithermClimate : public climate::Climate, public Component {
 public:
  EquithermClimate() = default;
  void setup() override;
  void dump_config() override;

  // Sensor inputs
  void set_outdoor_sensor(sensor::Sensor *sensor) { outdoor_sensor_ = sensor; }
  void set_indoor_sensor(sensor::Sensor *sensor) { indoor_sensor_ = sensor; }

  // Output (mutually exclusive - only one should be set)
  void set_ch_setpoint(number::Number *number) { ch_setpoint_ = number; }
  void set_heat_output(output::FloatOutput *output) { heat_output_ = output; }

  // Climate defaults
  void set_default_target_temperature(float temp) { default_target_temperature_ = temp; }

  // Heating curve parameters (delegated to heating curve)
  void set_hc(float hc) { heating_curve_.set_hc(hc); }
  void set_n(float n) { heating_curve_.set_n(n); }
  void set_shift(float shift) { heating_curve_.set_shift(shift); }

  // Output parameters
  void set_min_flow_temp(float temp) { heating_curve_.set_min_flow_temp(temp); }
  void set_max_flow_temp(float temp) { heating_curve_.set_max_flow_temp(temp); }
  void set_smoothing_threshold(float threshold) { smoothing_threshold_ = threshold; }

  // PID parameters
  void set_kp(float kp) { pid_controller_.kp_ = kp; }
  void set_ki(float ki) { pid_controller_.ki_ = ki; }
  void set_kd(float kd) { pid_controller_.kd_ = kd; }
  void set_min_integral(float min_integral) { pid_controller_.min_integral_ = min_integral; }
  void set_max_integral(float max_integral) { pid_controller_.max_integral_ = max_integral; }

  // Deadband parameters
  void set_threshold_low(float threshold) { pid_controller_.threshold_low_ = threshold; }
  void set_threshold_high(float threshold) { pid_controller_.threshold_high_ = threshold; }
  void set_kp_multiplier(float mult) { pid_controller_.kp_multiplier_ = mult; }
  void set_ki_multiplier(float mult) { pid_controller_.ki_multiplier_ = mult; }
  void set_kd_multiplier(float mult) { pid_controller_.kd_multiplier_ = mult; }

  // Fallback parameters (sensor failure handling)
  void set_fallback_outdoor_temp(float temp) { fallback_outdoor_temp_ = temp; }
  float get_fallback_outdoor_temp() const { return fallback_outdoor_temp_; }
  bool is_outdoor_fallback_active() const { return outdoor_fallback_active_; }
  bool is_indoor_fallback_active() const { return indoor_fallback_active_; }

  // State getters (for diagnostics)
  float get_base_curve_output() const { return base_curve_output_; }
  float get_final_flow_setpoint() const { return final_flow_setpoint_; }
  float get_raw_pid_correction() const { return raw_pid_correction_; }
  float get_gain_scheduling_ratio() const { return gain_scheduling_ratio_; }
  float get_scaled_correction() const { return raw_pid_correction_ * gain_scheduling_ratio_; }
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

  // Callback for diagnostic sensors
  void add_on_state_callback(std::function<void()> &&callback) { state_callback_.add(std::move(callback)); }

  // Force immediate recalculation (used by runtime tuning numbers)
  void recalculate(bool curve_changed) {
    if (curve_changed)
      this->compute_reference_sensitivity_();
    this->prev_smoothed_flow_ = NAN;  // Bypass smoothing on forced recalc
    this->compute_and_apply_(false);  // Don't tick PID on param change
  }

 protected:
  /// Override control to handle climate calls from HA
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller
  climate::ClimateTraits traits() override;

  void compute_and_apply_(bool update_pid = true);
  void compute_reference_sensitivity_();
  void write_setpoint_(float temp_c);
  void write_setpoint_off_();

  /// Outdoor temperature sensor (required)
  sensor::Sensor *outdoor_sensor_{nullptr};
  /// Indoor temperature sensor (required - for current_temperature display and room correction)
  sensor::Sensor *indoor_sensor_{nullptr};
  /// OpenTherm number output for direct °C control
  number::Number *ch_setpoint_{nullptr};
  /// Alternative: generic float output (normalized 0-1)
  output::FloatOutput *heat_output_{nullptr};

  /// Curve calculation engine
  HeatingCurve heating_curve_;
  /// PID controller for fine-tuning equitherm output
  PIDController pid_controller_;
  /// Default target temperature when no state restored
  float default_target_temperature_{20.0f};
  /// Minimum change to trigger output update (°C)
  float smoothing_threshold_{0.5f};
  /// Previous smoothed flow temperature for threshold comparison
  float prev_smoothed_flow_{NAN};
  /// Base curve output before corrections (for diagnostics)
  float base_curve_output_{NAN};
  /// Final flow setpoint sent to boiler (for diagnostics)
  float final_flow_setpoint_{NAN};
  /// Raw PID correction before gain scheduling (for diagnostics)
  float raw_pid_correction_{NAN};
  /// Current gain scheduling ratio (for diagnostics) - 1.0 when not active
  float gain_scheduling_ratio_{1.0f};
  /// Reference sensitivity for gain scheduling (computed in setup)
  float reference_sensitivity_{NAN};
  /// Previous target temperature for detecting changes
  float prev_target_temperature_{NAN};
  /// Fallback outdoor temperature when sensor fails (default 0°C - safe for winter)
  float fallback_outdoor_temp_{0.0f};
  /// Last known valid outdoor temperature for stale data window
  float last_valid_outdoor_temp_{NAN};
  /// Last known valid indoor temperature for display when sensor fails
  float last_valid_indoor_temp_{NAN};
  /// Whether outdoor sensor fallback is currently active
  bool outdoor_fallback_active_{false};
  /// Whether indoor sensor has failed (PID disabled, pure equitherm mode)
  bool indoor_fallback_active_{false};
  /// Timestamp of last valid outdoor sensor reading
  uint32_t last_valid_outdoor_time_{0};
  /// Timestamp of last valid indoor sensor reading
  uint32_t last_valid_indoor_time_{0};
  /// Callback for diagnostic sensors
  CallbackManager<void()> state_callback_;
};

}  // namespace equitherm
}  // namespace esphome
