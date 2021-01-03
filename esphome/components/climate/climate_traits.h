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
 *    - dry mode (removes humidity from air)
 *    - fan mode (only turns on fan)
 *  - supports away - away mode means that the climate device supports two different
 *      target temperature settings: one target temp setting for "away" mode and one for non-away mode.
 *  - supports boost - boost mode means that the climate device supports boost or turbo mode:
 *      it's device specific - usually a combination of settings that allow to reach setTemp in the lowest time
 *      possible
 *  - supports sleep - sleep mode means that the climate device supports sleep mode
 *      it's device specific - usually a silent mode with low fan for sleep_ use.
 *  - supports eco - eco mode means that the climate device supports eco mode
 *      it's device specific - usually a low power consumption setting.
 *  - supports action - if the climate device supports reporting the active
 *    current action of the device with the action property.
 *  - supports fan modes - optionally, if it has a fan which can be configured in different ways:
 *    - on, off, auto, high, medium, low, middle, focus, diffuse
 *  - supports swing modes - optionally, if it has a swing which can be configured in different ways:
 *    - off, both, vertical, horizontal
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
  void set_supports_fan_only_mode(bool supports_fan_only_mode);
  void set_supports_dry_mode(bool supports_dry_mode);
  void set_supports_away(bool supports_away);
  bool get_supports_away() const;
  void set_supports_boost(bool supports_boost);
  bool get_supports_boost() const;
  void set_supports_eco(bool supports_eco);
  bool get_supports_eco() const;
  void set_supports_sleep_(bool supports_sleep_);
  bool get_supports_sleep_() const;
  void set_supports_action(bool supports_action);
  bool get_supports_action() const;
  bool supports_mode(ClimateMode mode) const;
  void set_supports_fan_mode_on(bool supports_fan_mode_on);
  void set_supports_fan_mode_off(bool supports_fan_mode_off);
  void set_supports_fan_mode_auto(bool supports_fan_mode_auto);
  void set_supports_fan_mode_low(bool supports_fan_mode_low);
  void set_supports_fan_mode_medium(bool supports_fan_mode_medium);
  void set_supports_fan_mode_high(bool supports_fan_mode_high);
  void set_supports_fan_mode_middle(bool supports_fan_mode_middle);
  void set_supports_fan_mode_focus(bool supports_fan_mode_focus);
  void set_supports_fan_mode_diffuse(bool supports_fan_mode_diffuse);
  bool supports_fan_mode(ClimateFanMode fan_mode) const;
  bool get_supports_fan_modes() const;
  void set_supports_swing_mode_off(bool supports_swing_mode_off);
  void set_supports_swing_mode_both(bool supports_swing_mode_both);
  void set_supports_swing_mode_vertical(bool supports_swing_mode_vertical);
  void set_supports_swing_mode_horizontal(bool supports_swing_mode_horizontal);
  bool supports_swing_mode(ClimateSwingMode swing_mode) const;
  bool get_supports_swing_modes() const;

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
  bool supports_fan_only_mode_{false};
  bool supports_dry_mode_{false};
  bool supports_away_{false};
  bool supports_boost_{false};
  bool supports_eco_{false};
  bool supports_sleep__{false};
  bool supports_action_{false};
  bool supports_fan_mode_on_{false};
  bool supports_fan_mode_off_{false};
  bool supports_fan_mode_auto_{false};
  bool supports_fan_mode_low_{false};
  bool supports_fan_mode_medium_{false};
  bool supports_fan_mode_high_{false};
  bool supports_fan_mode_middle_{false};
  bool supports_fan_mode_focus_{false};
  bool supports_fan_mode_diffuse_{false};
  bool supports_swing_mode_off_{false};
  bool supports_swing_mode_both_{false};
  bool supports_swing_mode_vertical_{false};
  bool supports_swing_mode_horizontal_{false};

  float visual_min_temperature_{10};
  float visual_max_temperature_{30};
  float visual_temperature_step_{0.1};
};

}  // namespace climate
}  // namespace esphome
