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
  void set_humidity_sensor(sensor::Sensor *humidity_sensor);
  Trigger<> *get_idle_trigger() const;
  Trigger<> *get_cool_trigger() const;
  void set_supports_cool(bool supports_cool);
  Trigger<> *get_heat_trigger() const;
  void set_supports_heat(bool supports_heat);
  void set_normal_config(const BangBangClimateTargetTempConfig &normal_config);
  void set_away_config(const BangBangClimateTargetTempConfig &away_config);

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Change the away setting, will reset target temperatures to defaults.
  void change_away_(bool away);
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the state of this climate controller.
  void compute_state_();

  /// Switch the climate device to the given climate mode.
  void switch_to_action_(climate::ClimateAction action);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};
  /// The sensor used for getting the current humidity
  sensor::Sensor *humidity_sensor_{nullptr};

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

  BangBangClimateTargetTempConfig normal_config_{};
  bool supports_away_{false};
  BangBangClimateTargetTempConfig away_config_{};
};

}  // namespace bang_bang
}  // namespace esphome
