#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"
#include "equitherm_controller.h"
#include "equitherm_pid_controller.h"

namespace esphome {
namespace number {
class Number;  // Forward declaration
}
namespace equitherm_climate {

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

  // Equitherm curve parameters (delegated to controller)
  void set_slope(float slope) { controller_.set_slope(slope); }
  void set_exponent(float exp) { controller_.set_exponent(exp); }
  void set_shift(float shift) { controller_.set_shift(shift); }
  void set_target_diff_factor(float factor) { target_diff_factor_ = factor; }
  void set_room_error_clamp(float clamp) { room_error_clamp_ = clamp; }

  // Output parameters
  void set_t_min_flow(float temp) { controller_.set_t_min_flow(temp); }
  void set_t_max_flow(float temp) { controller_.set_t_max_flow(temp); }
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

  // State getters (for diagnostics)
  float get_flow_curve() const { return flow_curve_; }
  float get_current_setpoint() const { return current_setpoint_; }
  float get_pid_correction() const { return pid_correction_; }
  bool in_deadband() { return pid_controller_.in_deadband(); }

  // Parameter getters (for runtime tuning)
  float get_slope() const { return controller_.get_slope(); }
  float get_exponent() const { return controller_.get_exponent(); }
  float get_shift() const { return controller_.get_shift(); }
  float get_target_diff_factor() const { return target_diff_factor_; }
  float get_t_min_flow() const { return controller_.get_t_min_flow(); }
  float get_t_max_flow() const { return controller_.get_t_max_flow(); }
  float get_kp() const { return pid_controller_.kp_; }
  float get_ki() const { return pid_controller_.ki_; }
  float get_kd() const { return pid_controller_.kd_; }

  // Callback for diagnostic sensors
  void add_on_state_callback(std::function<void()> &&callback) { state_callback_.add(std::move(callback)); }

  // Force immediate recalculation (used by runtime tuning numbers)
  void recalculate() { this->update_equitherm_(); }

 protected:
  /// Override control to handle climate calls from HA
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller
  climate::ClimateTraits traits() override;

  void update_equitherm_();
  void schedule_update_();
  void write_setpoint_(float temp_c);

  /// Outdoor temperature sensor (required)
  sensor::Sensor *outdoor_sensor_;
  /// Indoor temperature sensor (required - for current_temperature display and room correction)
  sensor::Sensor *indoor_sensor_;
  /// OpenTherm number output for direct °C control
  number::Number *ch_setpoint_{nullptr};
  /// Alternative: generic float output (normalized 0-1)
  output::FloatOutput *heat_output_{nullptr};

  /// Curve calculation engine
  EquithermController controller_;
  /// PID controller for fine-tuning equitherm output
  EquithermPIDController pid_controller_;
  /// Default target temperature when no state restored
  float default_target_temperature_;
  /// Multiplier for room temperature error correction
  float target_diff_factor_{1.0f};
  /// Maximum room error correction clamp (°C)
  float room_error_clamp_{3.0f};
  /// Minimum change to trigger output update (°C)
  float smoothing_threshold_{0.5f};
  /// Last update timestamp for debouncing (milliseconds)
  uint32_t last_update_ms_{0};
  /// Flag to track if a deferred update is pending
  bool deferred_update_pending_{false};
  /// Previous equitherm result for smoothing threshold
  float prev_et_result_{NAN};
  /// Base equitherm curve output (for diagnostics) - NAN until first calculation
  float flow_curve_{NAN};
  /// Current calculated setpoint (for diagnostics) - NAN until first calculation
  float current_setpoint_{NAN};
  /// Current PID correction value (for diagnostics) - NAN until first calculation
  float pid_correction_{NAN};
  /// Callback for diagnostic sensors
  CallbackManager<void()> state_callback_;
};

}  // namespace equitherm_climate
}  // namespace esphome
