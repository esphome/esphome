#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "PID_v1.h"

namespace esphome {
namespace pid {

struct PidClimatePidConfig {
 public:
  PidClimatePidConfig();
  PidClimatePidConfig(unsigned int sample_time, float kp, float ki, float kd);

  unsigned int sample_time{200};
  float kp{NAN};
  float ki{NAN};
  float kd{NAN};
};

class PidClimate : public climate::Climate, public Component {
 public:
  PidClimate();
  void setup() override;

  void set_sensor(sensor::Sensor *sensor);
  Trigger<> *get_idle_trigger() const;
  Trigger<> *get_cool_trigger() const;
  void set_supports_cool(bool supports_cool);
  Trigger<> *get_heat_trigger() const;
  void set_supports_heat(bool supports_heat);
  void set_target_temperature(float target_temperature);
  void set_pid_config(const PidClimatePidConfig &config);
  void log_state();

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  void loop() override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the state of this climate controller.
  void compute_state_();
  void set_mode_(climate::ClimateMode new_mode);

  /// Switch the climate device to the given climate mode.
  void switch_to_action_(climate::ClimateAction action);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};
  /** The trigger to call when the controller should switch to idle mode.
   *
   * In idle mode, the controller is assumed to have both heating and cooling disabled.
   */
  Trigger<> *idle_trigger_;
  /** The trigger to call when the controller should switch to cooling mode.
   */
  Trigger<> *cool_trigger_;
  /** Whether the controller supports cooling.
   *
   * A false value for this attribute means that the controller has no cooling action
   * (for example a thermostat, where only heating and not-heating is possible).
   */
  bool supports_cool_{false};
  /** The trigger to call when the controller should switch to heating mode.
   *
   * A null value for this attribute means that the controller has no heating action
   * For example window blinds, where only cooling (blinds closed) and not-cooling
   * (blinds open) is possible.
   */
  Trigger<> *heat_trigger_{nullptr};
  bool supports_heat_{false};

  /** A reference to the trigger that was previously active.
   *
   * This is so that the previous trigger can be stopped before enabling a new one.
   */
  Trigger<> *prev_trigger_{nullptr};

  PidClimatePidConfig pid_config_{};
    
  double input_{NAN};
  double setpoint_{NAN};
  double output_{0};
  PID *pid_controller_{nullptr};
};

}  // namespace pid
}  // namespace esphome
