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
  Trigger<> *get_normal_trigger() const;
  void set_supports_normal(bool supports_normal);
  Trigger<> *get_eco_trigger() const;
  void set_supports_eco(bool supports_eco);
  Trigger<> *get_boost_trigger() const;
  void set_supports_boost(bool supports_boost);
  Trigger<> *get_comfort_trigger() const;
  void set_supports_comfort(bool supports_comfort);
  Trigger<> *get_sleep_trigger() const;
  void set_supports_sleep(bool supports_sleep);
  Trigger<> *get_auto_trigger() const;
  void set_supports_auto(bool supports_auto);
  Trigger<> *get_baby_trigger() const;
  void set_supports_baby(bool supports_baby);
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

  /** The trigger to call when the controller should switch to normal mode.
   */
  Trigger<> *normal_trigger_;
  /** Whether the controller supports normal mode.
   * A false value for this attribute means that the controller has no normal mode action
   */
  bool supports_normal_{false};

  /** The trigger to call when the controller should switch to eco mode.
   */
  Trigger<> *eco_trigger_;
  /** Whether the controller supports eco mode.
   * A false value for this attribute means that the controller has no eco mode action.
   */
  bool supports_eco_{false};

  /** The trigger to call when the controller should switch to boost mode.
   */
  Trigger<> *boost_trigger_;
  /** Whether the controller supports boost mode.
   * A false value for this attribute means that the controller has no boost mode action
   */
  bool supports_boost_{false};

  /** The trigger to call when the controller should switch to comfort mode.
   */
  Trigger<> *comfort_trigger_;
  /** Whether the controller supports comfort mode.
   * A false value for this attribute means that the controller has no comfort mode action.
   */
  bool supports_comfort_{false};

  /** The trigger to call when the controller should switch to sleep mode.
   */
  Trigger<> *sleep_trigger_;
  /** Whether the controller supports sleep mode.
   * A false value for this attribute means that the controller has no sleep mode action
   */
  bool supports_sleep_{false};

  /** The trigger to call when the controller should switch to auto mode.
   */
  Trigger<> *auto_trigger_;
  /** Whether the controller supports auto mode.
   * A false value for this attribute means that the controller has no auto mode action.
   */
  bool supports_auto_{false};

  /** The trigger to call when the controller should switch to baby mode.
   */
  Trigger<> *baby_trigger_;
  /** Whether the controller supports baby mode.
   * A false value for this attribute means that the controller has no baby mode action
   */
  bool supports_baby_{false};

  /** A reference to the trigger that was previously active.
   *
   * This is so that the previous trigger can be stopped before enabling a new one.
   */
  Trigger<> *prev_trigger_{nullptr};

  GenericHumidifierTargetHumidityConfig normal_config_{};
};

}  // namespace generic_humidifier
}  // namespace esphome
