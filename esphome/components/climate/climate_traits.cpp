#include "climate_traits.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

bool ClimateTraits::supports_mode(ClimateMode mode) const {
  switch (mode) {
    case CLIMATE_MODE_OFF:
      return true;
    case CLIMATE_MODE_AUTO:
      return this->supports_auto_mode_;
    case CLIMATE_MODE_COOL:
      return this->supports_cool_mode_;
    case CLIMATE_MODE_HEAT:
      return this->supports_heat_mode_;
    case CLIMATE_MODE_FAN_ONLY:
      return this->supports_fan_only_mode_;
    case CLIMATE_MODE_DRY:
      return this->supports_dry_mode_;
    default:
      return false;
  }
}
bool ClimateTraits::get_supports_current_temperature() const { return supports_current_temperature_; }
void ClimateTraits::set_supports_current_temperature(bool supports_current_temperature) {
  supports_current_temperature_ = supports_current_temperature;
}
bool ClimateTraits::get_supports_two_point_target_temperature() const { return supports_two_point_target_temperature_; }
void ClimateTraits::set_supports_two_point_target_temperature(bool supports_two_point_target_temperature) {
  supports_two_point_target_temperature_ = supports_two_point_target_temperature;
}
void ClimateTraits::set_supports_auto_mode(bool supports_auto_mode) { supports_auto_mode_ = supports_auto_mode; }
void ClimateTraits::set_supports_cool_mode(bool supports_cool_mode) { supports_cool_mode_ = supports_cool_mode; }
void ClimateTraits::set_supports_heat_mode(bool supports_heat_mode) { supports_heat_mode_ = supports_heat_mode; }
void ClimateTraits::set_supports_fan_only_mode(bool supports_fan_only_mode) {
  supports_fan_only_mode_ = supports_fan_only_mode;
}
void ClimateTraits::set_supports_dry_mode(bool supports_dry_mode) { supports_dry_mode_ = supports_dry_mode; }
void ClimateTraits::set_supports_away(bool supports_away) { supports_away_ = supports_away; }
void ClimateTraits::set_supports_boost(bool supports_boost) { supports_boost_ = supports_boost; }
void ClimateTraits::set_supports_eco(bool supports_eco) { supports_eco_ = supports_eco; }
void ClimateTraits::set_supports_sleep_(bool supports_sleep_) { supports_sleep__ = supports_sleep_; }
void ClimateTraits::set_supports_action(bool supports_action) { supports_action_ = supports_action; }
float ClimateTraits::get_visual_min_temperature() const { return visual_min_temperature_; }
void ClimateTraits::set_visual_min_temperature(float visual_min_temperature) {
  visual_min_temperature_ = visual_min_temperature;
}
float ClimateTraits::get_visual_max_temperature() const { return visual_max_temperature_; }
void ClimateTraits::set_visual_max_temperature(float visual_max_temperature) {
  visual_max_temperature_ = visual_max_temperature;
}
float ClimateTraits::get_visual_temperature_step() const { return visual_temperature_step_; }
int8_t ClimateTraits::get_temperature_accuracy_decimals() const {
  // use printf %g to find number of digits based on temperature step
  char buf[32];
  sprintf(buf, "%.5g", this->visual_temperature_step_);
  std::string str{buf};
  size_t dot_pos = str.find('.');
  if (dot_pos == std::string::npos)
    return 0;

  return str.length() - dot_pos - 1;
}
void ClimateTraits::set_visual_temperature_step(float temperature_step) { visual_temperature_step_ = temperature_step; }
bool ClimateTraits::get_supports_away() const { return supports_away_; }
bool ClimateTraits::get_supports_boost() const { return supports_boost_; }
bool ClimateTraits::get_supports_eco() const { return supports_eco_; }
bool ClimateTraits::get_supports_sleep_() const { return supports_sleep__; }
bool ClimateTraits::get_supports_action() const { return supports_action_; }

void ClimateTraits::set_supports_fan_mode_on(bool supports_fan_mode_on) {
  this->supports_fan_mode_on_ = supports_fan_mode_on;
}
void ClimateTraits::set_supports_fan_mode_off(bool supports_fan_mode_off) {
  this->supports_fan_mode_off_ = supports_fan_mode_off;
}
void ClimateTraits::set_supports_fan_mode_auto(bool supports_fan_mode_auto) {
  this->supports_fan_mode_auto_ = supports_fan_mode_auto;
}
void ClimateTraits::set_supports_fan_mode_low(bool supports_fan_mode_low) {
  this->supports_fan_mode_low_ = supports_fan_mode_low;
}
void ClimateTraits::set_supports_fan_mode_medium(bool supports_fan_mode_medium) {
  this->supports_fan_mode_medium_ = supports_fan_mode_medium;
}
void ClimateTraits::set_supports_fan_mode_high(bool supports_fan_mode_high) {
  this->supports_fan_mode_high_ = supports_fan_mode_high;
}
void ClimateTraits::set_supports_fan_mode_middle(bool supports_fan_mode_middle) {
  this->supports_fan_mode_middle_ = supports_fan_mode_middle;
}
void ClimateTraits::set_supports_fan_mode_focus(bool supports_fan_mode_focus) {
  this->supports_fan_mode_focus_ = supports_fan_mode_focus;
}
void ClimateTraits::set_supports_fan_mode_diffuse(bool supports_fan_mode_diffuse) {
  this->supports_fan_mode_diffuse_ = supports_fan_mode_diffuse;
}
bool ClimateTraits::supports_fan_mode(ClimateFanMode fan_mode) const {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_ON:
      return this->supports_fan_mode_on_;
    case climate::CLIMATE_FAN_OFF:
      return this->supports_fan_mode_off_;
    case climate::CLIMATE_FAN_AUTO:
      return this->supports_fan_mode_auto_;
    case climate::CLIMATE_FAN_LOW:
      return this->supports_fan_mode_low_;
    case climate::CLIMATE_FAN_MEDIUM:
      return this->supports_fan_mode_medium_;
    case climate::CLIMATE_FAN_HIGH:
      return this->supports_fan_mode_high_;
    case climate::CLIMATE_FAN_MIDDLE:
      return this->supports_fan_mode_middle_;
    case climate::CLIMATE_FAN_FOCUS:
      return this->supports_fan_mode_focus_;
    case climate::CLIMATE_FAN_DIFFUSE:
      return this->supports_fan_mode_diffuse_;
    default:
      return false;
  }
}
bool ClimateTraits::get_supports_fan_modes() const {
  return this->supports_fan_mode_on_ || this->supports_fan_mode_off_ || this->supports_fan_mode_auto_ ||
         this->supports_fan_mode_low_ || this->supports_fan_mode_medium_ || this->supports_fan_mode_high_ ||
         this->supports_fan_mode_middle_ || this->supports_fan_mode_focus_ || this->supports_fan_mode_diffuse_;
}
void ClimateTraits::set_supports_swing_mode_off(bool supports_swing_mode_off) {
  this->supports_swing_mode_off_ = supports_swing_mode_off;
}
void ClimateTraits::set_supports_swing_mode_both(bool supports_swing_mode_both) {
  this->supports_swing_mode_both_ = supports_swing_mode_both;
}
void ClimateTraits::set_supports_swing_mode_vertical(bool supports_swing_mode_vertical) {
  this->supports_swing_mode_vertical_ = supports_swing_mode_vertical;
}
void ClimateTraits::set_supports_swing_mode_horizontal(bool supports_swing_mode_horizontal) {
  this->supports_swing_mode_horizontal_ = supports_swing_mode_horizontal;
}
bool ClimateTraits::supports_swing_mode(ClimateSwingMode swing_mode) const {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_OFF:
      return this->supports_swing_mode_off_;
    case climate::CLIMATE_SWING_BOTH:
      return this->supports_swing_mode_both_;
    case climate::CLIMATE_SWING_VERTICAL:
      return this->supports_swing_mode_vertical_;
    case climate::CLIMATE_SWING_HORIZONTAL:
      return this->supports_swing_mode_horizontal_;
    default:
      return false;
  }
}
bool ClimateTraits::get_supports_swing_modes() const {
  return this->supports_swing_mode_off_ || this->supports_swing_mode_both_ || supports_swing_mode_vertical_ ||
         supports_swing_mode_horizontal_;
}
}  // namespace climate
}  // namespace esphome
