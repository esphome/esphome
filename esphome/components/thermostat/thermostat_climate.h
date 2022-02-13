#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace thermostat {

enum ThermostatClimateTimerIndex : size_t {
  TIMER_COOLING_MAX_RUN_TIME = 0,
  TIMER_COOLING_OFF = 1,
  TIMER_COOLING_ON = 2,
  TIMER_FAN_MODE = 3,
  TIMER_FANNING_OFF = 4,
  TIMER_FANNING_ON = 5,
  TIMER_HEATING_MAX_RUN_TIME = 6,
  TIMER_HEATING_OFF = 7,
  TIMER_HEATING_ON = 8,
  TIMER_IDLE_ON = 9,
};

struct ThermostatClimateTimer {
  const std::string name;
  bool active;
  uint32_t time;
  std::function<void()> func;
};

struct ThermostatClimateTargetTempConfig {
 public:
  ThermostatClimateTargetTempConfig();
  ThermostatClimateTargetTempConfig(float default_temperature);
  ThermostatClimateTargetTempConfig(float default_temperature_low, float default_temperature_high);

  float default_temperature{NAN};
  float default_temperature_low{NAN};
  float default_temperature_high{NAN};
  float cool_deadband_{NAN};
  float cool_overrun_{NAN};
  float heat_deadband_{NAN};
  float heat_overrun_{NAN};
};

class ThermostatClimate : public climate::Climate, public Component {
 public:
  ThermostatClimate();
  void setup() override;
  void dump_config() override;

  void set_default_mode(climate::ClimateMode default_mode);
  void set_set_point_minimum_differential(float differential);
  void set_cool_deadband(float deadband);
  void set_cool_overrun(float overrun);
  void set_heat_deadband(float deadband);
  void set_heat_overrun(float overrun);
  void set_supplemental_cool_delta(float delta);
  void set_supplemental_heat_delta(float delta);
  void set_cooling_maximum_run_time_in_sec(uint32_t time);
  void set_heating_maximum_run_time_in_sec(uint32_t time);
  void set_cooling_minimum_off_time_in_sec(uint32_t time);
  void set_cooling_minimum_run_time_in_sec(uint32_t time);
  void set_fan_mode_minimum_switching_time_in_sec(uint32_t time);
  void set_fanning_minimum_off_time_in_sec(uint32_t time);
  void set_fanning_minimum_run_time_in_sec(uint32_t time);
  void set_heating_minimum_off_time_in_sec(uint32_t time);
  void set_heating_minimum_run_time_in_sec(uint32_t time);
  void set_idle_minimum_time_in_sec(uint32_t time);
  void set_sensor(sensor::Sensor *sensor);
  void set_use_startup_delay(bool use_startup_delay);
  void set_supports_auto(bool supports_auto);
  void set_supports_heat_cool(bool supports_heat_cool);
  void set_supports_cool(bool supports_cool);
  void set_supports_dry(bool supports_dry);
  void set_supports_fan_only(bool supports_fan_only);
  void set_supports_fan_only_action_uses_fan_mode_timer(bool fan_only_action_uses_fan_mode_timer);
  void set_supports_fan_only_cooling(bool supports_fan_only_cooling);
  void set_supports_fan_with_cooling(bool supports_fan_with_cooling);
  void set_supports_fan_with_heating(bool supports_fan_with_heating);
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
  Trigger<> *get_supplemental_cool_action_trigger() const;
  Trigger<> *get_dry_action_trigger() const;
  Trigger<> *get_fan_only_action_trigger() const;
  Trigger<> *get_heat_action_trigger() const;
  Trigger<> *get_supplemental_heat_action_trigger() const;
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
  Trigger<> *get_temperature_change_trigger() const;
  /// Get current hysteresis values
  float cool_deadband();
  float cool_overrun();
  float heat_deadband();
  float heat_overrun();
  /// Call triggers based on updated climate states (modes/actions)
  void refresh();
  /// Returns true if a climate action/fan mode transition is being delayed
  bool climate_action_change_delayed();
  bool fan_mode_change_delayed();
  /// Returns the climate action that is being delayed (check climate_action_change_delayed(), first!)
  climate::ClimateAction delayed_climate_action();
  /// Returns the fan mode that is locked in (check fan_mode_change_delayed(), first!)
  climate::ClimateFanMode locked_fan_mode();
  /// Set point and hysteresis validation
  bool hysteresis_valid();  // returns true if valid
  void validate_target_temperature();
  void validate_target_temperatures();
  void validate_target_temperature_low();
  void validate_target_temperature_high();

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;

  /// Change the away setting, will reset target temperatures to defaults.
  void change_away_(bool away);

  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the required action of this climate controller.
  climate::ClimateAction compute_action_(bool ignore_timers = false);
  climate::ClimateAction compute_supplemental_action_();

  /// Switch the climate device to the given climate action.
  void switch_to_action_(climate::ClimateAction action, bool publish_state = true);
  void switch_to_supplemental_action_(climate::ClimateAction action);
  void trigger_supplemental_action_();

  /// Switch the climate device to the given climate fan mode.
  void switch_to_fan_mode_(climate::ClimateFanMode fan_mode, bool publish_state = true);

  /// Switch the climate device to the given climate mode.
  void switch_to_mode_(climate::ClimateMode mode, bool publish_state = true);

  /// Switch the climate device to the given climate swing mode.
  void switch_to_swing_mode_(climate::ClimateSwingMode swing_mode, bool publish_state = true);

  /// Check if the temperature change trigger should be called.
  void check_temperature_change_trigger_();

  /// Is the action ready to be called? Returns true if so
  bool idle_action_ready_();
  bool cooling_action_ready_();
  bool drying_action_ready_();
  bool fan_mode_ready_();
  bool fanning_action_ready_();
  bool heating_action_ready_();

  /// Start/cancel/get status of climate action timer
  void start_timer_(ThermostatClimateTimerIndex timer_index);
  bool cancel_timer_(ThermostatClimateTimerIndex timer_index);
  bool timer_active_(ThermostatClimateTimerIndex timer_index);
  uint32_t timer_duration_(ThermostatClimateTimerIndex timer_index);
  std::function<void()> timer_cbf_(ThermostatClimateTimerIndex timer_index);

  /// set_timeout() callbacks for various actions (see above)
  void cooling_max_run_time_timer_callback_();
  void cooling_off_timer_callback_();
  void cooling_on_timer_callback_();
  void fan_mode_timer_callback_();
  void fanning_off_timer_callback_();
  void fanning_on_timer_callback_();
  void heating_max_run_time_timer_callback_();
  void heating_off_timer_callback_();
  void heating_on_timer_callback_();
  void idle_on_timer_callback_();

  /// Check if cooling/fanning/heating actions are required; returns true if so
  bool cooling_required_();
  bool fanning_required_();
  bool heating_required_();
  bool supplemental_cooling_required_();
  bool supplemental_heating_required_();

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};

  /// Whether the controller supports auto/cooling/drying/fanning/heating.
  ///
  /// A false value for any given attribute means that the controller has no such action
  /// (for example a thermostat, where only heating and not-heating is possible).
  bool supports_auto_{false};
  bool supports_heat_cool_{false};
  bool supports_cool_{false};
  bool supports_dry_{false};
  bool supports_fan_only_{false};
  bool supports_heat_{false};
  /// Special flag -- enables fan_modes to share timer with fan_only climate action
  bool supports_fan_only_action_uses_fan_mode_timer_{false};
  /// Special flag -- enables fan to be switched based on target_temperature_high
  bool supports_fan_only_cooling_{false};
  /// Special flags -- enables fan_only action to be called with cooling/heating actions
  bool supports_fan_with_cooling_{false};
  bool supports_fan_with_heating_{false};

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

  /// Flags indicating if maximum allowable run time was exceeded
  bool cooling_max_runtime_exceeded_{false};
  bool heating_max_runtime_exceeded_{false};

  /// Used to start "off" delay timers at boot
  bool use_startup_delay_{false};

  /// setup_complete_ blocks modifying/resetting the temps immediately after boot
  bool setup_complete_{false};

  /// The trigger to call when the controller should switch to cooling action/mode.
  ///
  /// A null value for this attribute means that the controller has no cooling action
  /// For example electric heat, where only heating (power on) and not-heating
  /// (power off) is possible.
  Trigger<> *cool_action_trigger_{nullptr};
  Trigger<> *supplemental_cool_action_trigger_{nullptr};
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
  Trigger<> *supplemental_heat_action_trigger_{nullptr};
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

  /// The trigger to call when the target temperature(s) change(es).
  Trigger<> *temperature_change_trigger_{nullptr};

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
  climate::ClimateAction supplemental_action_{climate::CLIMATE_ACTION_OFF};
  climate::ClimateFanMode prev_fan_mode_{climate::CLIMATE_FAN_ON};
  climate::ClimateMode default_mode_{climate::CLIMATE_MODE_OFF};
  climate::ClimateMode prev_mode_{climate::CLIMATE_MODE_OFF};
  climate::ClimateSwingMode prev_swing_mode_{climate::CLIMATE_SWING_OFF};

  /// Store previously-known temperatures
  ///
  /// These are used to determine when the temperature change trigger/action needs to be called
  float prev_target_temperature_{NAN};
  float prev_target_temperature_low_{NAN};
  float prev_target_temperature_high_{NAN};

  /// Minimum differential required between set points
  float set_point_minimum_differential_{0};

  /// Hysteresis values used for computing climate actions
  float cooling_deadband_{0};
  float cooling_overrun_{0};
  float heating_deadband_{0};
  float heating_overrun_{0};

  /// Maximum allowable temperature deltas before engauging supplemental cooling/heating actions
  float supplemental_cool_delta_{0};
  float supplemental_heat_delta_{0};

  /// Minimum allowable duration in seconds for action timers
  const uint8_t min_timer_duration_{1};

  /// Temperature data for normal/home and away modes
  ThermostatClimateTargetTempConfig normal_config_{};
  ThermostatClimateTargetTempConfig away_config_{};

  /// Climate action timers
  std::vector<ThermostatClimateTimer> timer_{
      {"cool_run", false, 0, std::bind(&ThermostatClimate::cooling_max_run_time_timer_callback_, this)},
      {"cool_off", false, 0, std::bind(&ThermostatClimate::cooling_off_timer_callback_, this)},
      {"cool_on", false, 0, std::bind(&ThermostatClimate::cooling_on_timer_callback_, this)},
      {"fan_mode", false, 0, std::bind(&ThermostatClimate::fan_mode_timer_callback_, this)},
      {"fan_off", false, 0, std::bind(&ThermostatClimate::fanning_off_timer_callback_, this)},
      {"fan_on", false, 0, std::bind(&ThermostatClimate::fanning_on_timer_callback_, this)},
      {"heat_run", false, 0, std::bind(&ThermostatClimate::heating_max_run_time_timer_callback_, this)},
      {"heat_off", false, 0, std::bind(&ThermostatClimate::heating_off_timer_callback_, this)},
      {"heat_on", false, 0, std::bind(&ThermostatClimate::heating_on_timer_callback_, this)},
      {"idle_on", false, 0, std::bind(&ThermostatClimate::idle_on_timer_callback_, this)}};
};

}  // namespace thermostat
}  // namespace esphome
