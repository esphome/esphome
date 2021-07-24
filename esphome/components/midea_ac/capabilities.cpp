#include "esphome/core/log.h"
#include "capabilities.h"

namespace esphome {
namespace midea_ac {

bool Capabilities::read(const Frame &frame) {
  const uint8_t *data = frame.data() + 10;
  const uint8_t length = frame.size() - 12;

  uint8_t i = 2;
  uint8_t caps2Process = data[1];

  while (i < length - 2 && caps2Process) {
    if (data[i + 1] == 0x00 && data[i + 2] > 0) {
      switch (data[i]) {
        case 0x15:
          this->indoor_humidity_ = data[i + 3] != 0;
          break;
        case 0x18:
          this->silky_cool_ = data[i + 3] != 0;
          break;
        case 0x30:
          this->smart_eye_ = data[i + 3] == 1;
          break;
        case 0x32:
          this->wind_on_me_ = data[i + 3] == 1;
          break;
        case 0x33: 
          this->wind_of_me_ = data[i + 3] == 1;
          break;
        case 0x39: 
          this->active_clean_ = data[i + 3] == 1;
          break;
        case 0x42:
          this->one_key_no_wind_on_me_ = data[i + 3] == 1;
          break;
        case 0x43:
          this->breeze_control_ = data[i + 3] == 1;
          break;
      }
    }

    if (data[i + 1] == 0x02 && data[i + 2] > 0) {
      switch (data[i]) {
        case 0x10:        
          this->fanspeed_control_ = data[i + 3] != 1;
          break;
        case 0x12:
          this->eco_mode_ = data[i + 3] == 1;
          this->special_eco_ = data[i + 3] == 2;
          break;
        case 0x13:
          this->frost_protection_mode_ = data[i + 3] == 1;
          break;
        case 0x14:
          switch (data[i + 3]) {
            case 0:
              this->heat_mode_ = false;
              this->cool_mode_ = true;
              this->dry_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 1:
              this->cool_mode_ = true;
              this->heat_mode_= true;
              this->dry_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 2:
              this->cool_mode_ = false;
              this->dry_mode_ = false;
              this->heat_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 3:
              this->cool_mode_ = true;
              this->dry_mode_ = false;
              this->heat_mode_ = false;
              this->auto_mode_ = false;
              break;
          }
          break;
        case 0x15:
          switch (data[i + 3]) {
            case 0:
              this->leftright_fan_ = false;
              this->updown_fan_ = true;
              break;
            case 1:
              this->leftright_fan_ = true;
              this->updown_fan_ = true;
              break;
            case 2:
              this->leftright_fan_ = false;
              this->updown_fan_ = false;
              break;
            case 3:
              this->leftright_fan_ = true;
              this->updown_fan_ = false;
              break;
          }
          break;
        case 0x16: 
          switch (data[i + 3]) {
            case 0:
            case 1:
              this->power_cal_ = false;
              this->power_cal_setting_ = false;
              break;
            case 2:
              this->power_cal_ = true;
              this->power_cal_setting_ = false;
              break;
            case 3:
              this->power_cal_ = true;
              this->power_cal_setting_ = true;
              break;
          }
          break;
        case 0x17: 
          switch (data[i + 3]) {
            case 0:
              this->nest_check_ = false;
              this->nest_need_change_ = false;
              break;
            case 1: 
            case 2:
              this->nest_check_ = true;
              this->nest_need_change_ = false;
              break;
            case 3: 
              this->nest_check_ = false;
              this->nest_need_change_ = true;
              break;
            case 4: 
              this->nest_check_ = true;
              this->nest_need_change_ = true;
              break;
          }
          break;
        case 0x19:
          this->electric_aux_heating_ = data[i + 3] == 1;
          break;
        case 0x1A: 
          switch (data[i + 3]) {
            case 0:
              this->turbo_heat_ = false;
              this->turbo_cool_ = true;
              break;
            case 1:
              this->turbo_heat_ = true;
              this->turbo_cool_ = true;
              break;
            case 2:
              this->turbo_heat_ = false;
              this->turbo_cool_ = false;
              break;
            case 3:
              this->turbo_heat_ = true;
              this->turbo_cool_ = false;
              break;
          }
          break;
        case 0x1F:
          switch (data[i + 3]) {
            case 0: 
              this->auto_set_humidity_ = false;
              this->manual_set_humidity_ = false;
              break;
            case 1:
              this->auto_set_humidity_ = true;
              this->manual_set_humidity_ = false;
              break;
            case 2:
              this->auto_set_humidity_ = true;
              this->manual_set_humidity_ = true;
              break;
            case 3:
              this->auto_set_humidity_ = false;
              this->manual_set_humidity_ = true;
              break;
          }
          break;
        case 0x22:
          this->unit_changeable_ = data[i + 3] == 0;
          break;
        case 0x24:
          this->light_control_ = data[i + 3];
          break;
        case 0x25:
          if (data[i + 2] >= 6) {
            this->min_temp_cool_ = static_cast<float>(data[i + 3]) * 0.5f;
            this->max_temp_cool_ = static_cast<float>(data[i + 4]) * 0.5f;
            this->min_temp_auto_ = static_cast<float>(data[i + 5]) * 0.5f;
            this->max_temp_auto_ = static_cast<float>(data[i + 6]) * 0.5f;
            this->min_temp_heat_ = static_cast<float>(data[i + 7]) * 0.5f;
            this->max_temp_heat_ = static_cast<float>(data[i + 8]) * 0.5f;

            if (data[i + 2] > 6)
              this->decimals_ = data[i + 9] > 0;
            else
              this->decimals_ = data[i + 5] != 0;
          break;
        case 0x2C:
          this->buzzer_ = data[i + 3] != 0;
          break;
        }
      }
    }
    // Increment cursor and decrement capabilities to process
    i += (3 + data[i + 2]);
    caps2Process--;
  }
  
  if (length - i > 1)
    return data[length - 2] > 0;
  
  this->is_ready_ = true;
  return false;
}

const std::string Capabilities::FROST_PROTECTION = "FROST_PROTECTION";

void Capabilities::to_climate_traits(ClimateTraits &traits) const {
  /* TEMPERATURES */
  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);

  /* MODES */
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_FAN_ONLY);
  if (this->auto_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT_COOL);
  if (this->cool_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  if (this->heat_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  if (this->dry_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);

  /* FAN MODES */
  traits.set_supported_fan_modes({
      ClimateFanMode::CLIMATE_FAN_AUTO,
      ClimateFanMode::CLIMATE_FAN_LOW,
      ClimateFanMode::CLIMATE_FAN_MEDIUM,
      ClimateFanMode::CLIMATE_FAN_HIGH,
  });

  /* SWING MODES */
  traits.set_supported_swing_modes({
      ClimateSwingMode::CLIMATE_SWING_OFF,
      ClimateSwingMode::CLIMATE_SWING_VERTICAL,
  });

  /* PRESETS */
  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
  if (this->turbo_cool() || this->turbo_heat())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  if (this->eco_mode())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);

  /* CUSTOM PRESETS */
  if (this->frost_protection_mode())
    traits.add_supported_custom_preset(FROST_PROTECTION);
}


static const char *bool_to_string(bool val) { return val ? "TRUE" : "FALSE"; }

void print_capabilities(const char *tag, const Capabilities &capabilities) {
  ESP_LOGCONFIG(tag, "AIR CONDITIONER CAPABILITIES:");
  ESP_LOGCONFIG(tag, "  AUTO SET HUMIDITY: %s", bool_to_string(capabilities.auto_set_humidity()));
  ESP_LOGCONFIG(tag, "  AUTO MODE: %s", bool_to_string(capabilities.auto_mode()));
  ESP_LOGCONFIG(tag, "  ACTIVE CLEAN: %s", bool_to_string(capabilities.active_clean()));
  ESP_LOGCONFIG(tag, "  BREEZE CONTROL: %s", bool_to_string(capabilities.breeze_control()));
  ESP_LOGCONFIG(tag, "  BUZZER: %s", bool_to_string(capabilities.buzzer()));
  ESP_LOGCONFIG(tag, "  COOL MODE: %s", bool_to_string(capabilities.cool_mode()));
  ESP_LOGCONFIG(tag, "  DECIMALS: %s", bool_to_string(capabilities.decimals()));
  ESP_LOGCONFIG(tag, "  DRY MODE: %s", bool_to_string(capabilities.dry_mode()));
  ESP_LOGCONFIG(tag, "  ECO MODE: %s", bool_to_string(capabilities.eco_mode()));
  ESP_LOGCONFIG(tag, "  ELECTRIC AUX HEATING: %s", bool_to_string(capabilities.electric_aux_heating()));
  ESP_LOGCONFIG(tag, "  FANSPEED CONTROL: %s", bool_to_string(capabilities.fanspeed_control()));
  ESP_LOGCONFIG(tag, "  FROST PROTECTION MODE: %s", bool_to_string(capabilities.frost_protection_mode()));
  ESP_LOGCONFIG(tag, "  HEAT MODE: %s", bool_to_string(capabilities.heat_mode()));
  ESP_LOGCONFIG(tag, "  INDOOR HUMIDITY: %s", bool_to_string(capabilities.indoor_humidity()));
  ESP_LOGCONFIG(tag, "  LEFTRIGHT FAN: %s", bool_to_string(capabilities.leftright_fan()));
  ESP_LOGCONFIG(tag, "  LIGHT CONTROL: %s", bool_to_string(capabilities.light_control()));
  ESP_LOGCONFIG(tag, "  MANUAL SET HUMIDITY: %s", bool_to_string(capabilities.manual_set_humidity()));
  ESP_LOGCONFIG(tag, "  MAX TEMP AUTO: %f", bool_to_string(capabilities.max_temp_auto()));
  ESP_LOGCONFIG(tag, "  MAX TEMP COOL: %f", bool_to_string(capabilities.max_temp_cool()));
  ESP_LOGCONFIG(tag, "  MAX TEMP HEAT: %f", bool_to_string(capabilities.max_temp_heat()));
  ESP_LOGCONFIG(tag, "  MIN TEMP AUTO: %f", bool_to_string(capabilities.min_temp_auto()));
  ESP_LOGCONFIG(tag, "  MIN TEMP COOL: %f", bool_to_string(capabilities.min_temp_cool()));
  ESP_LOGCONFIG(tag, "  MIN TEMP HEAT: %f", bool_to_string(capabilities.min_temp_heat()));
  ESP_LOGCONFIG(tag, "  NEST CHECK: %s", bool_to_string(capabilities.nest_check()));
  ESP_LOGCONFIG(tag, "  NEST NEED CHANGE: %s", bool_to_string(capabilities.nest_need_change()));
  ESP_LOGCONFIG(tag, "  ONE KEY NO WIND ON ME: %s", bool_to_string(capabilities.one_key_no_wind_on_me()));
  ESP_LOGCONFIG(tag, "  POWER CAL: %s", bool_to_string(capabilities.power_cal()));
  ESP_LOGCONFIG(tag, "  POWER CAL SETTING: %s", bool_to_string(capabilities.power_cal_setting()));
  ESP_LOGCONFIG(tag, "  SILKY COOL: %s", bool_to_string(capabilities.silky_cool()));
  ESP_LOGCONFIG(tag, "  SMART EYE: %s", bool_to_string(capabilities.smart_eye()));
  ESP_LOGCONFIG(tag, "  SOUND: %s", bool_to_string(capabilities.sound()));
  ESP_LOGCONFIG(tag, "  SPECIAL ECO: %s", bool_to_string(capabilities.special_eco()));
  ESP_LOGCONFIG(tag, "  TURBO COOL: %s", bool_to_string(capabilities.turbo_cool()));
  ESP_LOGCONFIG(tag, "  TURBO HEAT: %s", bool_to_string(capabilities.turbo_heat()));
  ESP_LOGCONFIG(tag, "  UNIT CHANGEABLE: %s", bool_to_string(capabilities.unit_changeable()));
  ESP_LOGCONFIG(tag, "  UPDOWN FAN: %s", bool_to_string(capabilities.updown_fan()));
  ESP_LOGCONFIG(tag, "  WIND OF ME: %s", bool_to_string(capabilities.wind_of_me()));
  ESP_LOGCONFIG(tag, "  WIND ON ME: %s", bool_to_string(capabilities.wind_on_me()));
}

}  // namespace midea_ac
}  // namespace esphome
