#pragma once
#include "esphome/components/midea_dongle/midea_frame.h"
#include "esphome/components/climate/climate_traits.h"

namespace esphome {
namespace midea_ac {

using midea_dongle::Frame;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;

class Capabilities {
 public:
  static const std::string FREEZE_PROTECTION;
  static const std::string SILENT;
  static const std::string TURBO;

  // Read from frames
  bool read(const Frame &frame);
  // Is ready for reading Climate traits
  bool is_ready() const { return this->is_ready_; }
  // To ClimateTraits
  void to_climate_traits(ClimateTraits &traits) const;
  // Control humidity
  bool auto_set_humidity() const { return this->auto_set_humidity_; };
  bool auto_mode() const { return this->auto_mode_; };
  bool active_clean() const { return this->active_clean_; };
  bool breeze_control() const { return this->breeze_control_; };
  bool buzzer() const { return this->buzzer_; }
  bool cool_mode() const { return this->cool_mode_; }
  bool decimals() const { return this->decimals_; }
  bool dry_mode() const { return this->dry_mode_; }
  bool eco_mode() const { return this->eco_mode_; }
  bool electric_aux_heating() const { return this->electric_aux_heating_; }
  bool fanspeed_control() const { return this->fanspeed_control_; }
  bool frost_protection_mode() const { return this->frost_protection_mode_; }
  bool heat_mode() const { return this->heat_mode_; }
  bool indoor_humidity() const { return this->indoor_humidity_; }
  bool leftright_fan() const { return this->leftright_fan_; }
  // Ability to turn LED display off
  bool light_control() const { return this->light_control_; }
  // Control humidity
  bool manual_set_humidity() const { return this->manual_set_humidity_; }
  float max_temp_auto() const { return this->max_temp_auto_; }
  float max_temp_cool() const { return this->max_temp_cool_; }
  float max_temp_heat() const { return this->max_temp_heat_; }
  float min_temp_auto() const { return this->min_temp_auto_; }
  float min_temp_cool() const { return this->min_temp_cool_; }
  float min_temp_heat() const { return this->min_temp_heat_; }
  bool nest_check() const { return this->nest_check_; }
  bool nest_need_change() const { return this->nest_need_change_; }
  bool one_key_no_wind_on_me() const { return this->one_key_no_wind_on_me_; }
  bool power_cal() const { return this->power_cal_; }
  bool power_cal_setting() const { return this->power_cal_setting_; }
  bool silky_cool() const { return this->silky_cool_; }
  // Intelligent eye function
  bool smart_eye() const { return this->smart_eye_; }
  bool sound() const { return this->sound_; }
  bool special_eco() const { return this->special_eco_; }
  bool turbo_cool() const { return this->turbo_cool_; }
  bool turbo_heat() const { return this->turbo_heat_; }
  // Temperature unit can be changed between Celsius and Fahrenheit
  bool unit_changeable() const { return this->unit_changeable_; }
  bool updown_fan() const { return this->updown_fan_; }
  bool wind_of_me() const { return this->wind_of_me_; }
  bool wind_on_me() const { return this->wind_on_me_; }
 protected:
  bool auto_set_humidity_{false};
  bool auto_mode_{false};
  bool active_clean_{false};
  bool breeze_control_{false};
  bool buzzer_{false};
  bool cool_mode_{false};
  bool decimals_{false};
  bool dry_mode_{false};
  bool eco_mode_{false};
  bool electric_aux_heating_{false};
  bool fanspeed_control_{true};
  bool frost_protection_mode_{false};
  bool heat_mode_{false};
  bool indoor_humidity_{false};
  bool leftright_fan_{false};
  bool light_control_{false};
  bool manual_set_humidity_{false};
  float max_temp_auto_{31};
  float max_temp_cool_{31};
  float max_temp_heat_{31};
  float min_temp_auto_{16};
  float min_temp_cool_{16};
  float min_temp_heat_{16};
  bool nest_check_{false};
  bool nest_need_change_{false};
  bool one_key_no_wind_on_me_{false};
  bool power_cal_{false};
  bool power_cal_setting_{false};
  bool silky_cool_{false};
  bool smart_eye_{false};
  bool sound_{false};
  bool special_eco_{false};
  bool turbo_cool_{false};
  bool turbo_heat_{false};
  bool unit_changeable_{false};
  bool updown_fan_{false};
  bool wind_of_me_{false};
  bool wind_on_me_{false};
  // is filled?
  bool is_ready_{false};
};

void print_capabilities(const char *tag, const Capabilities &capabilities);

}  // namespace midea_ac
}  // namespace esphome
