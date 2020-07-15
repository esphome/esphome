#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace thermostat {

struct ThermostatClimateTargetTempConfig {
 public:
  ThermostatClimateTargetTempConfig();
  ThermostatClimateTargetTempConfig(float default_temperature);
  ThermostatClimateTargetTempConfig(float default_temperature_low, float default_temperature_high);

  float default_temperature{NAN};
  float default_temperature_low{NAN};
  float default_temperature_high{NAN};
  float hysteresis{NAN};
};

class ThermostatClimate : public climate::Climate, public Component {
 public:
  ThermostatClimate();
  void setup() override;
  void dump_config() override;

  void set_hysteresis(float hysteresis);
  void set_sensor(sensor::Sensor *sensor);
  void set_supports_auto(bool supports_auto);
  void set_supports_cool(bool supports_cool);
  void set_supports_dry(bool supports_dry);
  void set_supports_fan_only(bool supports_fan_only);
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
  void set_supports_swing_mode_both(bool supports_swing_mode_both);
  void set_supports_swing_mode_horizontal(bool supports_swing_mode_horizontal);
  void set_supports_swing_mode_off(bool supports_swing_mode_off);
  void set_supports_swing_mode_vertical(bool supports_swing_mode_vertical);
  void set_supports_two_points(bool supports_two_points);

  void set_normal_config(const ThermostatClimateTargetTempConfig &normal_config);
  void set_away_config(const ThermostatClimateTargetTempConfig &away_config);

  Trigger<> *get_cool_action_trigger() const;
  Trigger<> *get_dry_action_trigger() const;
  Trigger<> *get_fan_only_action_trigger() const;
  Trigger<> *get_heat_action_trigger() const;
  Trigger<> *get_idle_action_trigger() const;
  Trigger<> *get_auto_mode_trigger() const;
  Trigger<> *get_cool_mode_trigger() const;
  Trigger<> *get_dry_mode_trigger() const;
  Trigger<> *get_fan_only_mode_trigger() const;
  Trigger<> *get_heat_mode_trigger() const;
  Trigger<> *get_off_mode_trigger() const;
  Trigger<> *get_fan_mode_on_trigger() const;
  Trigger<> *get_fan_mode_off_trigger() const;
  Trigger<> *get_fan_mode_auto_trigger() const;
  Trigger<> *get_fan_mode_low_trigger() const;
  Trigger<> *get_fan_mode_medium_trigger() const;
  Trigger<> *get_fan_mode_high_trigger() const;
  Trigger<> *get_fan_mode_middle_trigger() const;
  Trigger<> *get_fan_mode_focus_trigger() const;
  Trigger<> *get_fan_mode_diffuse_trigger() const;
  Trigger<> *get_swing_mode_both_trigger() const;
  Trigger<> *get_swing_mode_horizontal_trigger() const;
  Trigger<> *get_swing_mode_off_trigger() const;
  Trigger<> *get_swing_mode_vertical_trigger() const;
  /// Get current hysteresis value
  float hysteresis();
  /// Call triggers based on updated climate states (modes/actions)
  void refresh();

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;

  /// Change the away setting, will reset target temperatures to defaults.
  void change_away_(bool away);

  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the required action of this climate controller.
  climate::ClimateAction compute_action_();

  /// Switch the climate device to the given climate action.
  void switch_to_action_(climate::ClimateAction action);

  /// Switch the climate device to the given climate fan mode.
  void switch_to_fan_mode_(climate::ClimateFanMode fan_mode);

  /// Switch the climate device to the given climate mode.
  void switch_to_mode_(climate::ClimateMode mode);

  /// Switch the climate device to the given climate swing mode.
  void switch_to_swing_mode_(climate::ClimateSwingMode swing_mode);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};

  /// Whether the controller supports auto/cooling/drying/fanning/heating.
  ///
  /// A false value for any given attribute means that the controller has no such action
  /// (for example a thermostat, where only heating and not-heating is possible).
  bool supports_auto_{false};
  bool supports_cool_{false};
  bool supports_dry_{false};
  bool supports_fan_only_{false};
  bool supports_heat_{false};

  /// Whether the controller supports turning on or off just the fan.
  ///
  /// A false value for either attribute means that the controller has no fan on/off action
  /// (for example a thermostat, where independent control of the fan is not possible).
  bool supports_fan_mode_on_{false};
  bool supports_fan_mode_off_{false};

  /// Whether the controller supports fan auto mode.
  ///
  /// A false value for this attribute means that the controller has no fan-auto action
  /// (for example a thermostat, where independent control of the fan is not possible).
  bool supports_fan_mode_auto_{false};

  /// Whether the controller supports various fan speeds and/or positions.
  ///
  /// A false value for any given attribute means that the controller has no such fan action.
  bool supports_fan_mode_low_{false};
  bool supports_fan_mode_medium_{false};
  bool supports_fan_mode_high_{false};
  bool supports_fan_mode_middle_{false};
  bool supports_fan_mode_focus_{false};
  bool supports_fan_mode_diffuse_{false};

  /// Whether the controller supports various swing modes.
  ///
  /// A false value for any given attribute means that the controller has no such swing mode.
  bool supports_swing_mode_both_{false};
  bool supports_swing_mode_off_{false};
  bool supports_swing_mode_horizontal_{false};
  bool supports_swing_mode_vertical_{false};

  /// Whether the controller supports two set points
  ///
  /// A false value means that the controller has no such support.
  bool supports_two_points_{false};

  /// Whether the controller supports an "away" mode
  ///
  /// A false value means that the controller has no such mode.
  bool supports_away_{false};

  /// The trigger to call when the controller should switch to cooling action/mode.
  ///
  /// A null value for this attribute means that the controller has no cooling action
  /// For example electric heat, where only heating (power on) and not-heating
  /// (power off) is possible.
  Trigger<> *cool_action_trigger_{nullptr};
  Trigger<> *cool_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to dry (dehumidification) mode.
  ///
  /// In dry mode, the controller is assumed to have both heating and cooling disabled,
  /// although the system may use its cooling mechanism to achieve drying.
  Trigger<> *dry_action_trigger_{nullptr};
  Trigger<> *dry_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to heating action/mode.
  ///
  /// A null value for this attribute means that the controller has no heating action
  /// For example window blinds, where only cooling (blinds closed) and not-cooling
  /// (blinds open) is possible.
  Trigger<> *heat_action_trigger_{nullptr};
  Trigger<> *heat_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to auto mode.
  ///
  /// In auto mode, the controller will enable heating/cooling as necessary and switch
  /// to idle when the temperature is within the thresholds/set points.
  Trigger<> *auto_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to idle action/off mode.
  ///
  /// In these actions/modes, the controller is assumed to have both heating and cooling disabled.
  Trigger<> *idle_action_trigger_{nullptr};
  Trigger<> *off_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to fan-only action/mode.
  ///
  /// In fan-only mode, the controller is assumed to have both heating and cooling disabled.
  /// The system should activate the fan only.
  Trigger<> *fan_only_action_trigger_{nullptr};
  Trigger<> *fan_only_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch on the fan.
  Trigger<> *fan_mode_on_trigger_{nullptr};

  /// The trigger to call when the controller should switch off the fan.
  Trigger<> *fan_mode_off_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "auto" mode.
  Trigger<> *fan_mode_auto_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "low" speed.
  Trigger<> *fan_mode_low_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "medium" speed.
  Trigger<> *fan_mode_medium_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "high" speed.
  Trigger<> *fan_mode_high_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "middle" position.
  Trigger<> *fan_mode_middle_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "focus" position.
  Trigger<> *fan_mode_focus_trigger_{nullptr};

  /// The trigger to call when the controller should switch the fan to "diffuse" position.
  Trigger<> *fan_mode_diffuse_trigger_{nullptr};

  /// The trigger to call when the controller should switch the swing mode to "both".
  Trigger<> *swing_mode_both_trigger_{nullptr};

  /// The trigger to call when the controller should switch the swing mode to "off".
  Trigger<> *swing_mode_off_trigger_{nullptr};

  /// The trigger to call when the controller should switch the swing mode to "horizontal".
  Trigger<> *swing_mode_horizontal_trigger_{nullptr};

  /// The trigger to call when the controller should switch the swing mode to "vertical".
  Trigger<> *swing_mode_vertical_trigger_{nullptr};

  /// A reference to the trigger that was previously active.
  ///
  /// This is so that the previous trigger can be stopped before enabling a new one
  /// for each climate category (mode, action, fan_mode, swing_mode).
  Trigger<> *prev_action_trigger_{nullptr};
  Trigger<> *prev_fan_mode_trigger_{nullptr};
  Trigger<> *prev_mode_trigger_{nullptr};
  Trigger<> *prev_swing_mode_trigger_{nullptr};

  /// Store previously-known states
  ///
  /// These are used to determine when a trigger/action needs to be called
  climate::ClimateFanMode prev_fan_mode_{climate::CLIMATE_FAN_ON};
  climate::ClimateMode prev_mode_{climate::CLIMATE_MODE_OFF};
  climate::ClimateSwingMode prev_swing_mode_{climate::CLIMATE_SWING_OFF};

  /// Temperature data for normal/home and away modes
  ThermostatClimateTargetTempConfig normal_config_{};
  ThermostatClimateTargetTempConfig away_config_{};

  /// Hysteresis value used for computing climate actions
  float hysteresis_{0};

  /// setup_complete_ blocks modifying/resetting the temps immediately after boot
  bool setup_complete_{false};
};

}  // namespace thermostat
}  // namespace esphome
