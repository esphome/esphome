#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bang_bang {

struct BangBangClimateTargetTempConfig {
 public:
  BangBangClimateTargetTempConfig();
  BangBangClimateTargetTempConfig(float default_temperature_low, float default_temperature_high);

  float default_temperature_low{NAN};
  float default_temperature_high{NAN};
};

class BangBangClimate : public climate::Climate, public Component {
 public:
  BangBangClimate();
  void setup() override;
  void dump_config() override;

  void set_sensor(sensor::Sensor *sensor);
  void set_supports_cool(bool supports_cool);
  void set_supports_heat(bool supports_heat);
  void set_supports_fan_mode_on(bool supports_fan_mode_on);
  void set_supports_fan_mode_off(bool supports_fan_mode_off);
  void set_supports_fan_mode_auto(bool supports_fan_mode_auto);
  void set_supports_fan_mode_low(bool supports_fan_mode_low);
  void set_supports_fan_mode_medium(bool supports_fan_mode_medium);
  void set_supports_fan_mode_high(bool supports_fan_mode_high);
  void set_supports_fan_mode_middle(bool supports_fan_mode_middle);
  void set_supports_fan_mode_focus(bool supports_fan_mode_focus);
  void set_supports_fan_mode_diffuse(bool supports_fan_mode_diffuse);
  void set_normal_config(const BangBangClimateTargetTempConfig &normal_config);
  void set_away_config(const BangBangClimateTargetTempConfig &away_config);

  Trigger<> *get_idle_trigger() const;
  Trigger<> *get_cool_trigger() const;
  Trigger<> *get_heat_trigger() const;
  Trigger<> *get_fan_mode_on_trigger() const;
  Trigger<> *get_fan_mode_off_trigger() const;
  Trigger<> *get_fan_mode_auto_trigger() const;
  Trigger<> *get_fan_mode_low_trigger() const;
  Trigger<> *get_fan_mode_medium_trigger() const;
  Trigger<> *get_fan_mode_high_trigger() const;
  Trigger<> *get_fan_mode_middle_trigger() const;
  Trigger<> *get_fan_mode_focus_trigger() const;
  Trigger<> *get_fan_mode_diffuse_trigger() const;

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Change the away setting, will reset target temperatures to defaults.
  void change_away_(bool away);
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the state of this climate controller.
  void compute_state_();

  /// Switch the climate device to the given climate action.
  void switch_to_action_(climate::ClimateAction action);

  /// Switch the climate device to the given climate fan mode.
  void switch_to_fan_mode_(climate::ClimateFanMode fan_mode);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};
  /** Whether the controller supports cooling.
   *
   * A false value for this attribute means that the controller has no cooling action
   * (for example a thermostat, where only heating and not-heating is possible).
   */
  bool supports_cool_{false};
  bool supports_heat_{false};
  /** Whether the controller supports turning on or off just the fan.
   *
   * A false value for either attribute means that the controller has no fan on/off action
   * (for example a thermostat, where independent control of the fan is not possible).
   */
  bool supports_fan_mode_on_{false};
  bool supports_fan_mode_off_{false};
  /** Whether the controller supports fan auto mode.
   *
   * A false value for this attribute means that the controller has no fan-auto action
   * (for example a thermostat, where independent control of the fan is not possible).
   */
  bool supports_fan_mode_auto_{false};
  /** Whether the controller supports various fan speed(s)/position(s).
   *
   * A false value for any given attribute means that the controller has no such fan action.
   */
  bool supports_fan_mode_low_{false};
  bool supports_fan_mode_medium_{false};
  bool supports_fan_mode_high_{false};
  bool supports_fan_mode_middle_{false};
  bool supports_fan_mode_focus_{false};
  bool supports_fan_mode_diffuse_{false};
  /** Whether the controller supports an "away" mode
   *
   * A false value means that the controller has no such mode.
   */
  bool supports_away_{false};
  /** The trigger to call when the controller should switch to idle mode.
   *
   * In idle mode, the controller is assumed to have both heating and cooling disabled.
   */
  Trigger<> *idle_trigger_{nullptr};
  /** The trigger to call when the controller should switch to cooling mode.
   *
   * A null value for this attribute means that the controller has no cooling action
   * For example electric heat, where only heating (power on) and not-heating
   * (power off) is possible.
   */
  Trigger<> *cool_trigger_{nullptr};
  /** The trigger to call when the controller should switch to heating mode.
   *
   * A null value for this attribute means that the controller has no heating action
   * For example window blinds, where only cooling (blinds closed) and not-cooling
   * (blinds open) is possible.
   */
  Trigger<> *heat_trigger_{nullptr};
  /** The trigger to call when the controller should switch on the fan.
   */
  Trigger<> *fan_mode_on_trigger_{nullptr};
  /** The trigger to call when the controller should switch off the fan.
   */
  Trigger<> *fan_mode_off_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "auto" mode.
   */
  Trigger<> *fan_mode_auto_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "low" speed.
   */
  Trigger<> *fan_mode_low_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "medium" speed.
   */
  Trigger<> *fan_mode_medium_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "high" speed.
   */
  Trigger<> *fan_mode_high_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "middle" position.
   */
  Trigger<> *fan_mode_middle_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "focus" position.
   */
  Trigger<> *fan_mode_focus_trigger_{nullptr};
  /** The trigger to call when the controller should switch the fan to "diffuse" position.
   */
  Trigger<> *fan_mode_diffuse_trigger_{nullptr};
  /** A reference to the trigger that was previously active.
   *
   * This is so that the previous trigger can be stopped before enabling a new one.
   */
  Trigger<> *prev_action_trigger_{nullptr};
  Trigger<> *prev_fan_mode_trigger_{nullptr};

  climate::ClimateFanMode prev_fan_mode_{climate::CLIMATE_FAN_ON};

  BangBangClimateTargetTempConfig normal_config_{};
  BangBangClimateTargetTempConfig away_config_{};
};

}  // namespace bang_bang
}  // namespace esphome
