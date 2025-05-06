#pragma once

#include "esphome/core/helpers.h"
#include "climate_mode.h"
#include <set>

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
 *  - supports action - if the climate device supports reporting the active
 *    current action of the device with the action property.
 *  - supports fan modes - optionally, if it has a fan which can be configured in different ways:
 *    - on, off, auto, high, medium, low, middle, focus, diffuse, quiet
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
  bool get_supports_current_temperature() const { return this->supports_current_temperature_; }
  void set_supports_current_temperature(bool supports_current_temperature) {
    this->supports_current_temperature_ = supports_current_temperature;
  }
  bool get_supports_current_humidity() const { return this->supports_current_humidity_; }
  void set_supports_current_humidity(bool supports_current_humidity) {
    this->supports_current_humidity_ = supports_current_humidity;
  }
  bool get_supports_two_point_target_temperature() const { return this->supports_two_point_target_temperature_; }
  void set_supports_two_point_target_temperature(bool supports_two_point_target_temperature) {
    this->supports_two_point_target_temperature_ = supports_two_point_target_temperature;
  }
  bool get_supports_target_humidity() const { return this->supports_target_humidity_; }
  void set_supports_target_humidity(bool supports_target_humidity) {
    this->supports_target_humidity_ = supports_target_humidity;
  }
  void set_supported_modes(std::set<ClimateMode> modes) { this->supported_modes_ = std::move(modes); }
  void add_supported_mode(ClimateMode mode) { this->supported_modes_.insert(mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_auto_mode(bool supports_auto_mode) { set_mode_support_(CLIMATE_MODE_AUTO, supports_auto_mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_cool_mode(bool supports_cool_mode) { set_mode_support_(CLIMATE_MODE_COOL, supports_cool_mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_heat_mode(bool supports_heat_mode) { set_mode_support_(CLIMATE_MODE_HEAT, supports_heat_mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_heat_cool_mode(bool supported) { set_mode_support_(CLIMATE_MODE_HEAT_COOL, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_fan_only_mode(bool supports_fan_only_mode) {
    set_mode_support_(CLIMATE_MODE_FAN_ONLY, supports_fan_only_mode);
  }
  ESPDEPRECATED("This method is deprecated, use set_supported_modes() instead", "v1.20")
  void set_supports_dry_mode(bool supports_dry_mode) { set_mode_support_(CLIMATE_MODE_DRY, supports_dry_mode); }
  bool supports_mode(ClimateMode mode) const { return this->supported_modes_.count(mode); }
  const std::set<ClimateMode> &get_supported_modes() const { return this->supported_modes_; }

  void set_supports_action(bool supports_action) { this->supports_action_ = supports_action; }
  bool get_supports_action() const { return this->supports_action_; }

  void set_supported_fan_modes(std::set<ClimateFanMode> modes) { this->supported_fan_modes_ = std::move(modes); }
  void add_supported_fan_mode(ClimateFanMode mode) { this->supported_fan_modes_.insert(mode); }
  void add_supported_custom_fan_mode(const std::string &mode) { this->supported_custom_fan_modes_.insert(mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_on(bool supported) { set_fan_mode_support_(CLIMATE_FAN_ON, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_off(bool supported) { set_fan_mode_support_(CLIMATE_FAN_OFF, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_auto(bool supported) { set_fan_mode_support_(CLIMATE_FAN_AUTO, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_low(bool supported) { set_fan_mode_support_(CLIMATE_FAN_LOW, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_medium(bool supported) { set_fan_mode_support_(CLIMATE_FAN_MEDIUM, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_high(bool supported) { set_fan_mode_support_(CLIMATE_FAN_HIGH, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_middle(bool supported) { set_fan_mode_support_(CLIMATE_FAN_MIDDLE, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_focus(bool supported) { set_fan_mode_support_(CLIMATE_FAN_FOCUS, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_fan_modes() instead", "v1.20")
  void set_supports_fan_mode_diffuse(bool supported) { set_fan_mode_support_(CLIMATE_FAN_DIFFUSE, supported); }
  bool supports_fan_mode(ClimateFanMode fan_mode) const { return this->supported_fan_modes_.count(fan_mode); }
  bool get_supports_fan_modes() const {
    return !this->supported_fan_modes_.empty() || !this->supported_custom_fan_modes_.empty();
  }
  const std::set<ClimateFanMode> &get_supported_fan_modes() const { return this->supported_fan_modes_; }

  void set_supported_custom_fan_modes(std::set<std::string> supported_custom_fan_modes) {
    this->supported_custom_fan_modes_ = std::move(supported_custom_fan_modes);
  }
  const std::set<std::string> &get_supported_custom_fan_modes() const { return this->supported_custom_fan_modes_; }
  bool supports_custom_fan_mode(const std::string &custom_fan_mode) const {
    return this->supported_custom_fan_modes_.count(custom_fan_mode);
  }

  void set_supported_presets(std::set<ClimatePreset> presets) { this->supported_presets_ = std::move(presets); }
  void add_supported_preset(ClimatePreset preset) { this->supported_presets_.insert(preset); }
  void add_supported_custom_preset(const std::string &preset) { this->supported_custom_presets_.insert(preset); }
  bool supports_preset(ClimatePreset preset) const { return this->supported_presets_.count(preset); }
  bool get_supports_presets() const { return !this->supported_presets_.empty(); }
  const std::set<climate::ClimatePreset> &get_supported_presets() const { return this->supported_presets_; }

  void set_supported_custom_presets(std::set<std::string> supported_custom_presets) {
    this->supported_custom_presets_ = std::move(supported_custom_presets);
  }
  const std::set<std::string> &get_supported_custom_presets() const { return this->supported_custom_presets_; }
  bool supports_custom_preset(const std::string &custom_preset) const {
    return this->supported_custom_presets_.count(custom_preset);
  }

  void set_supported_swing_modes(std::set<ClimateSwingMode> modes) { this->supported_swing_modes_ = std::move(modes); }
  void add_supported_swing_mode(ClimateSwingMode mode) { this->supported_swing_modes_.insert(mode); }
  ESPDEPRECATED("This method is deprecated, use set_supported_swing_modes() instead", "v1.20")
  void set_supports_swing_mode_off(bool supported) { set_swing_mode_support_(CLIMATE_SWING_OFF, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_swing_modes() instead", "v1.20")
  void set_supports_swing_mode_both(bool supported) { set_swing_mode_support_(CLIMATE_SWING_BOTH, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_swing_modes() instead", "v1.20")
  void set_supports_swing_mode_vertical(bool supported) { set_swing_mode_support_(CLIMATE_SWING_VERTICAL, supported); }
  ESPDEPRECATED("This method is deprecated, use set_supported_swing_modes() instead", "v1.20")
  void set_supports_swing_mode_horizontal(bool supported) {
    set_swing_mode_support_(CLIMATE_SWING_HORIZONTAL, supported);
  }
  bool supports_swing_mode(ClimateSwingMode swing_mode) const { return this->supported_swing_modes_.count(swing_mode); }
  bool get_supports_swing_modes() const { return !this->supported_swing_modes_.empty(); }
  const std::set<ClimateSwingMode> &get_supported_swing_modes() const { return this->supported_swing_modes_; }

  float get_visual_min_temperature() const { return this->visual_min_temperature_; }
  void set_visual_min_temperature(float visual_min_temperature) {
    this->visual_min_temperature_ = visual_min_temperature;
  }
  float get_visual_max_temperature() const { return this->visual_max_temperature_; }
  void set_visual_max_temperature(float visual_max_temperature) {
    this->visual_max_temperature_ = visual_max_temperature;
  }
  float get_visual_target_temperature_step() const { return this->visual_target_temperature_step_; }
  float get_visual_current_temperature_step() const { return this->visual_current_temperature_step_; }
  void set_visual_target_temperature_step(float temperature_step) {
    this->visual_target_temperature_step_ = temperature_step;
  }
  void set_visual_current_temperature_step(float temperature_step) {
    this->visual_current_temperature_step_ = temperature_step;
  }
  void set_visual_temperature_step(float temperature_step) {
    this->visual_target_temperature_step_ = temperature_step;
    this->visual_current_temperature_step_ = temperature_step;
  }
  int8_t get_target_temperature_accuracy_decimals() const;
  int8_t get_current_temperature_accuracy_decimals() const;

  float get_visual_min_humidity() const { return this->visual_min_humidity_; }
  void set_visual_min_humidity(float visual_min_humidity) { this->visual_min_humidity_ = visual_min_humidity; }
  float get_visual_max_humidity() const { return this->visual_max_humidity_; }
  void set_visual_max_humidity(float visual_max_humidity) { this->visual_max_humidity_ = visual_max_humidity; }

 protected:
  void set_mode_support_(climate::ClimateMode mode, bool supported) {
    if (supported) {
      this->supported_modes_.insert(mode);
    } else {
      this->supported_modes_.erase(mode);
    }
  }
  void set_fan_mode_support_(climate::ClimateFanMode mode, bool supported) {
    if (supported) {
      this->supported_fan_modes_.insert(mode);
    } else {
      this->supported_fan_modes_.erase(mode);
    }
  }
  void set_swing_mode_support_(climate::ClimateSwingMode mode, bool supported) {
    if (supported) {
      this->supported_swing_modes_.insert(mode);
    } else {
      this->supported_swing_modes_.erase(mode);
    }
  }

  bool supports_current_temperature_{false};
  bool supports_current_humidity_{false};
  bool supports_two_point_target_temperature_{false};
  bool supports_target_humidity_{false};
  std::set<climate::ClimateMode> supported_modes_ = {climate::CLIMATE_MODE_OFF};
  bool supports_action_{false};
  std::set<climate::ClimateFanMode> supported_fan_modes_;
  std::set<climate::ClimateSwingMode> supported_swing_modes_;
  std::set<climate::ClimatePreset> supported_presets_;
  std::set<std::string> supported_custom_fan_modes_;
  std::set<std::string> supported_custom_presets_;

  float visual_min_temperature_{10};
  float visual_max_temperature_{30};
  float visual_target_temperature_step_{0.1};
  float visual_current_temperature_step_{0.1};
  float visual_min_humidity_{30};
  float visual_max_humidity_{99};
};

}  // namespace climate
}  // namespace esphome
