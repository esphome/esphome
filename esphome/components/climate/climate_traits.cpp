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
void ClimateTraits::set_supports_away(bool supports_away) { supports_away_ = supports_away; }
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
bool ClimateTraits::get_supports_action() const { return supports_action_; }

}  // namespace climate
}  // namespace esphome
