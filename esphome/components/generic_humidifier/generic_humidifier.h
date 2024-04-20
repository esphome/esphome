#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/humidifier/humidifier.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace generic_humidifier {

struct GenericHumidifierTargetHumidityConfig {
 public:
  GenericHumidifierTargetHumidityConfig();
  GenericHumidifierTargetHumidityConfig(float default_humidity);

  float default_humidity{NAN};
};

class GenericHumidifier : public humidifier::Humidifier, public Component {
 public:
  GenericHumidifier();
  void setup() override;
  void dump_config() override;

  void set_sensor(sensor::Sensor *sensor);
  Trigger<> *get_level_1_trigger() const;
  void set_supports_level_1(bool supports_level_1);
  Trigger<> *get_level_2_trigger() const;
  void set_supports_level_2(bool supports_level_2);
  Trigger<> *get_level_3_trigger() const;
  void set_supports_level_3(bool supports_level_3);
  Trigger<> *get_preset_trigger() const;
  void set_supports_preset(bool supports_preset);
  void set_normal_config(const GenericHumidifierTargetHumidityConfig &normal_config);

 protected:
  /// Override control to change settings of the climate device.
  void control(const humidifier::HumidifierCall &call) override;
  /// Return the traits of this controller.
  humidifier::HumidifierTraits traits() override;

  /// Re-compute the state of this climate controller.
  void compute_state_();

  /// Switch the climate device to the given climate mode.
  void switch_to_action_(humidifier::HumidifierAction action);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};

  /** The trigger to call when the controller should switch to level 1.
   */
  Trigger<> *level_1_trigger_;
  /** Whether the controller supports level 1.
   *
   * A false value for this attribute means that the controller has no level 1 action
   */
  bool supports_level_1_{false};

    /** The trigger to call when the controller should switch to level 2.
   */
  Trigger<> *level_2_trigger_;
  /** Whether the controller supports level 2.
   *
   * A false value for this attribute means that the controller has no level 2 action.
   */
  bool supports_level_2_{false};

    /** The trigger to call when the controller should switch to level 3.
   */
  Trigger<> *level_3_trigger_;
  /** Whether the controller supports level 3.
   *
   * A false value for this attribute means that the controller has no level 3 action
   */
  bool supports_level_3_{false};

  
  /** The trigger to call when the controller should switch to preset mode.
   *
   * A null value for this attribute means that the controller has no preset action
   */
  Trigger<> *preset_trigger_{nullptr};
  bool supports_preset_{false};
  /** A reference to the trigger that was previously active.
   *
   * This is so that the previous trigger can be stopped before enabling a new one.
   */
  Trigger<> *prev_trigger_{nullptr};

  GenericHumidifierTargetHumidityConfig normal_config_{};
};

}  // namespace generic_humidifier
}  // namespace esphome
