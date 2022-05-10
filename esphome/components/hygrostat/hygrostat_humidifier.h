#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/humidifier/humidifier.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hygrostat {

enum HygrostatHumidifierTimerIndex : size_t {
  TIMER_DEHUMIDIFYING_MAX_RUN_TIME = 0,
  TIMER_DEHUMIDIFYING_OFF = 1,
  TIMER_DEHUMIDIFYING_ON = 2,
  TIMER_HUMIDIFYING_MAX_RUN_TIME = 3,
  TIMER_HUMIDIFYING_OFF = 4,
  TIMER_HUMIDIFYING_ON = 5,
  TIMER_IDLE_ON = 6,
};

struct HygrostatHumidifierTimer {
  const std::string name;
  bool active;
  uint32_t time;
  std::function<void()> func;
};

struct HygrostatHumidifierTargetTempConfig {
 public:
  HygrostatHumidifierTargetTempConfig();
  HygrostatHumidifierTargetTempConfig(float default_humidity);
  HygrostatHumidifierTargetTempConfig(float default_humidity_low, float default_humidity_high);

  float default_humidity{NAN};
  float default_humidity_low{NAN};
  float default_humidity_high{NAN};
  float dehumidify_deadband_{NAN};
  float dehumidify_overrun_{NAN};
  float humidify_deadband_{NAN};
  float humidify_overrun_{NAN};
};

class HygrostatHumidifier : public humidifier::Humidifier, public Component {
 public:
  HygrostatHumidifier();
  void setup() override;
  void dump_config() override;

  void set_default_mode(humidifier::HumidifierMode default_mode);
  void set_set_point_minimum_differential(float differential);
  void set_dehumidify_deadband(float deadband);
  void set_dehumidify_overrun(float overrun);
  void set_humidify_deadband(float deadband);
  void set_humidify_overrun(float overrun);
  void set_dehumidifying_maximum_run_time_in_sec(uint32_t time);
  void set_humidifying_maximum_run_time_in_sec(uint32_t time);
  void set_dehumidifying_minimum_off_time_in_sec(uint32_t time);
  void set_dehumidifying_minimum_run_time_in_sec(uint32_t time);
  void set_humidifying_minimum_off_time_in_sec(uint32_t time);
  void set_humidifying_minimum_run_time_in_sec(uint32_t time);
  void set_idle_minimum_time_in_sec(uint32_t time);
  void set_sensor(sensor::Sensor *sensor);
  void set_use_startup_delay(bool use_startup_delay);
  void set_supports_auto(bool supports_auto);
  void set_supports_humidify_dehumidify(bool supports_humidify_dehumidify);
  void set_supports_dehumidify(bool supports_dehumidify);
  void set_supports_humidify(bool supports_humidify);
  void set_supports_two_points(bool supports_two_points);

  void set_normal_config(const HygrostatHumidifierTargetTempConfig &normal_config);
  void set_away_config(const HygrostatHumidifierTargetTempConfig &away_config);

  Trigger<> *get_dehumidify_action_trigger() const;
  Trigger<> *get_supplemental_dehumidify_action_trigger() const;
  Trigger<> *get_humidify_action_trigger() const;
  Trigger<> *get_supplemental_humidify_action_trigger() const;
  Trigger<> *get_idle_action_trigger() const;
  Trigger<> *get_auto_mode_trigger() const;
  Trigger<> *get_dehumidify_mode_trigger() const;
  Trigger<> *get_humidify_mode_trigger() const;
  Trigger<> *get_off_mode_trigger() const;
  Trigger<> *get_humidity_change_trigger() const;
  /// Get current hysteresis values
  float dehumidify_deadband();
  float dehumidify_overrun();
  float humidify_deadband();
  float humidify_overrun();
  /// Call triggers based on updated humidifier states (modes/actions)
  void refresh();
  /// Returns true if a humidifier action mode transition is being delayed
  bool humidifier_action_change_delayed();
  /// Returns the humidifier action that is being delayed (check humidifier_action_change_delayed(), first!)
  humidifier::HumidifierAction delayed_humidifier_action();
  /// Set point and hysteresis validation
  bool hysteresis_valid();  // returns true if valid
  void validate_target_humidity();
  void validate_target_humiditys();
  void validate_target_humidity_low();
  void validate_target_humidity_high();

 protected:
  /// Override control to change settings of the humidifier device.
  void control(const humidifier::HumidifierCall &call) override;

  /// Change the away setting, will reset target humiditys to defaults.
  void change_away_(bool away);

  /// Return the traits of this controller.
  humidifier::HumidifierTraits traits() override;

  /// Re-compute the required action of this humidifier controller.
  humidifier::HumidifierAction compute_action_(bool ignore_timers = false);
  humidifier::HumidifierAction compute_supplemental_action_();

  /// Switch the humidifier device to the given humidifier action.
  void switch_to_action_(humidifier::HumidifierAction action, bool publish_state = true);
  void switch_to_supplemental_action_(humidifier::HumidifierAction action);
  void trigger_supplemental_action_();

  /// Switch the humidifier device to the given humidifier mode.
  void switch_to_mode_(humidifier::HumidifierMode mode, bool publish_state = true);

  /// Check if the humidity change trigger should be called.
  void check_humidity_change_trigger_();

  /// Is the action ready to be called? Returns true if so
  bool idle_action_ready_();
  bool dehumidifying_action_ready_();
  bool humidifying_action_ready_();

  /// Start/cancel/get status of humidifier action timer
  void start_timer_(HygrostatHumidifierTimerIndex timer_index);
  bool cancel_timer_(HygrostatHumidifierTimerIndex timer_index);
  bool timer_active_(HygrostatHumidifierTimerIndex timer_index);
  uint32_t timer_duration_(HygrostatHumidifierTimerIndex timer_index);
  std::function<void()> timer_cbf_(HygrostatHumidifierTimerIndex timer_index);

  /// set_timeout() callbacks for various actions (see above)
  void dehumidifying_max_run_time_timer_callback_();
  void dehumidifying_off_timer_callback_();
  void dehumidifying_on_timer_callback_();
  void humidifying_max_run_time_timer_callback_();
  void humidifying_off_timer_callback_();
  void humidifying_on_timer_callback_();
  void idle_on_timer_callback_();

  /// Check if dehumidifying/humidifying actions are required; returns true if so
  bool dehumidifying_required_();
  bool humidifying_required_();
  bool supplemental_dehumidifying_required_();
  bool supplemental_humidifying_required_();

  /// The sensor used for getting the current humidity
  sensor::Sensor *sensor_{nullptr};

  /// Whether the controller supports auto/dehumidifying/humidifying.
  ///
  /// A false value for any given attribute means that the controller has no such action
  /// (for example a hygrostat, where only humidifying and not-humidifying is possible).
  bool supports_auto_{false};
  bool supports_humidify_dehumidify_{false};
  bool supports_dehumidify_{false};
  bool supports_humidify_{false};

  /// Whether the controller supports two set points
  ///
  /// A false value means that the controller has no such support.
  bool supports_two_points_{false};

  /// Whether the controller supports an "away" mode
  ///
  /// A false value means that the controller has no such mode.
  bool supports_away_{false};

  /// Flags indicating if maximum allowable run time was exceeded
  bool dehumidifying_max_runtime_exceeded_{false};
  bool humidifying_max_runtime_exceeded_{false};

  /// Used to start "off" delay timers at boot
  bool use_startup_delay_{false};

  /// setup_complete_ blocks modifying/resetting the temps immediately after boot
  bool setup_complete_{false};

  /// The trigger to call when the controller should switch to dehumidifying action/mode.
  ///
  /// A null value for this attribute means that the controller has no dehumidifying action
  Trigger<> *dehumidify_action_trigger_{nullptr};
  Trigger<> *supplemental_dehumidify_action_trigger_{nullptr};
  Trigger<> *dehumidify_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to humidifying action/mode.
  ///
  /// A null value for this attribute means that the controller has no humidifying action
  /// For example window blinds, where only dehumidifying (blinds closed) and not-dehumidifying
  /// (blinds open) is possible.
  Trigger<> *humidify_action_trigger_{nullptr};
  Trigger<> *supplemental_humidify_action_trigger_{nullptr};
  Trigger<> *humidify_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to auto mode.
  ///
  /// In auto mode, the controller will enable humidifying/dehumidifying as necessary and switch
  /// to idle when the humidity is within the thresholds/set points.
  Trigger<> *auto_mode_trigger_{nullptr};

  /// The trigger to call when the controller should switch to idle action/off mode.
  ///
  /// In these actions/modes, the controller is assumed to have both humidifying and dehumidifying disabled.
  Trigger<> *idle_action_trigger_{nullptr};
  Trigger<> *off_mode_trigger_{nullptr};

  /// The trigger to call when the target humidity(s) change(es).
  Trigger<> *humidity_change_trigger_{nullptr};

  /// A reference to the trigger that was previously active.
  ///
  /// This is so that the previous trigger can be stopped before enabling a new one
  /// for each humidifier category (mode, action).
  Trigger<> *prev_action_trigger_{nullptr};
  Trigger<> *prev_mode_trigger_{nullptr};

  /// Store previously-known states
  ///
  /// These are used to determine when a trigger/action needs to be called
  humidifier::HumidifierAction supplemental_action_{humidifier::HUMIDIFIER_ACTION_OFF};
  humidifier::HumidifierMode default_mode_{humidifier::HUMIDIFIER_MODE_OFF};
  humidifier::HumidifierMode prev_mode_{humidifier::HUMIDIFIER_MODE_OFF};

  /// Store previously-known humiditys
  ///
  /// These are used to determine when the humidity change trigger/action needs to be called
  float prev_target_humidity_{NAN};
  float prev_target_humidity_low_{NAN};
  float prev_target_humidity_high_{NAN};

  /// Minimum differential required between set points
  float set_point_minimum_differential_{0};

  /// Hysteresis values used for computing humidifier actions
  float dehumidifying_deadband_{0};
  float dehumidifying_overrun_{0};
  float humidifying_deadband_{0};
  float humidifying_overrun_{0};

  /// Minimum allowable duration in seconds for action timers
  const uint8_t min_timer_duration_{1};

  /// Humidity data for normal/home and away modes
  HygrostatHumidifierTargetTempConfig normal_config_{};
  HygrostatHumidifierTargetTempConfig away_config_{};

  /// Humidifier action timers
  std::vector<HygrostatHumidifierTimer> timer_{
      {"dehumidify_run", false, 0, std::bind(&HygrostatHumidifier::dehumidifying_max_run_time_timer_callback_, this)},
      {"dehumidify_off", false, 0, std::bind(&HygrostatHumidifier::dehumidifying_off_timer_callback_, this)},
      {"dehumidify_on", false, 0, std::bind(&HygrostatHumidifier::dehumidifying_on_timer_callback_, this)},
      {"humidify_run", false, 0, std::bind(&HygrostatHumidifier::humidifying_max_run_time_timer_callback_, this)},
      {"humidify_off", false, 0, std::bind(&HygrostatHumidifier::humidifying_off_timer_callback_, this)},
      {"humidify_on", false, 0, std::bind(&HygrostatHumidifier::humidifying_on_timer_callback_, this)},
      {"idle_on", false, 0, std::bind(&HygrostatHumidifier::idle_on_timer_callback_, this)}};
};

}  // namespace hygrostat
}  // namespace esphome
