#pragma once

#include "climate_mode.h"

namespace esphome {
namespace climate {

/** This class contains all static data for climate devices.
 *
 * All climate devices must support these features:
 *  - OFF mode
 *  - Target Temperature
 *
 * All other properties and modes are optional and the integration must mark
 * each of them as supported by setting the appropriate flag here.
 *
 *  - supports current temperature - if the climate device supports reporting a current temperature
 *  - supports two point target temperature - if the climate device's target temperature should be
 *     split in target_temperature_low and target_temperature_high instead of just the single target_temperature
 *  - supports modes:
 *    - auto mode (automatic control)
 *    - cool mode (lowers current temperature)
 *    - heat mode (increases current temperature)
 *  - supports away - away mode means that the climate device supports two different
 *      target temperature settings: one target temp setting for "away" mode and one for non-away mode.
 *  - supports action - if the climate device supports reporting the active
 *    current action of the device with the action property.
 *
 * This class also contains static data for the climate device display:
 *  - visual min/max temperature - tells the frontend what range of temperatures the climate device
 *     should display (gauge min/max values)
 *  - temperature step - the step with which to increase/decrease target temperature.
 *     This also affects with how many decimal places the temperature is shown
 */
class ClimateTraits {
 public:
  bool get_supports_current_temperature() const;
  void set_supports_current_temperature(bool supports_current_temperature);
  bool get_supports_two_point_target_temperature() const;
  void set_supports_two_point_target_temperature(bool supports_two_point_target_temperature);
  void set_supports_auto_mode(bool supports_auto_mode);
  void set_supports_cool_mode(bool supports_cool_mode);
  void set_supports_heat_mode(bool supports_heat_mode);
  void set_supports_away(bool supports_away);
  bool get_supports_away() const;
  void set_supports_action(bool supports_action);
  bool get_supports_action() const;
  bool supports_mode(ClimateMode mode) const;

  float get_visual_min_temperature() const;
  void set_visual_min_temperature(float visual_min_temperature);
  float get_visual_max_temperature() const;
  void set_visual_max_temperature(float visual_max_temperature);
  float get_visual_temperature_step() const;
  int8_t get_temperature_accuracy_decimals() const;
  void set_visual_temperature_step(float temperature_step);

 protected:
  bool supports_current_temperature_{false};
  bool supports_two_point_target_temperature_{false};
  bool supports_auto_mode_{false};
  bool supports_cool_mode_{false};
  bool supports_heat_mode_{false};
  bool supports_away_{false};
  bool supports_action_{false};

  float visual_min_temperature_{10};
  float visual_max_temperature_{30};
  float visual_temperature_step_{0.1};
};

}  // namespace climate
}  // namespace esphome
