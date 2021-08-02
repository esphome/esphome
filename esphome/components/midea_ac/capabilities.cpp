#include "esphome/core/log.h"
#include "capabilities.h"
#include <pgmspace.h>
#include <string>

namespace esphome {
namespace midea_ac {

static uint8_t s_safe_(const uint8_t *data, uint8_t length, uint8_t idx) { return (idx < length) ? data[idx] : 0; }

bool Capabilities::read(const Frame &frame) {
  if (frame.size() < 14)
    return false;

  const uint8_t *data = frame.data() + 10;
  const uint8_t length = frame.size() - 12;

  uint8_t i = 2;
  uint8_t caps2_process = s_safe_(data, length, 1);

  while (i < length - 2 && caps2_process) {
    if (s_safe_(data, length, i + 1) == 0x00 && s_safe_(data, length, i + 2) > 0) {
      switch (s_safe_(data, length, i)) {
        case 0x15:
          this->indoor_humidity_ = s_safe_(data, length, i + 3) != 0;
          break;
        case 0x18:
          this->silky_cool_ = s_safe_(data, length, i + 3) != 0;
          break;
        case 0x30:
          this->smart_eye_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x32:
          this->wind_on_me_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x33:
          this->wind_of_me_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x39:
          this->active_clean_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x42:
          this->one_key_no_wind_on_me_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x43:
          this->breeze_control_ = s_safe_(data, length, i + 3) == 1;
          break;
      }
    }

    if (s_safe_(data, length, i + 1) == 0x02 && s_safe_(data, length, i + 2) > 0) {
      switch (s_safe_(data, length, i)) {
        case 0x10:
          this->fanspeed_control_ = s_safe_(data, length, i + 3) != 1;
          break;
        case 0x12:
          this->eco_mode_ = s_safe_(data, length, i + 3) == 1;
          this->special_eco_ = s_safe_(data, length, i + 3) == 2;
          break;
        case 0x13:
          this->frost_protection_mode_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x14:
          switch (s_safe_(data, length, i + 3)) {
            case 0:
              this->heat_mode_ = false;
              this->cool_mode_ = true;
              this->dry_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 1:
              this->cool_mode_ = true;
              this->heat_mode_ = true;
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
          switch (s_safe_(data, length, i + 3)) {
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
          switch (s_safe_(data, length, i + 3)) {
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
          switch (s_safe_(data, length, i + 3)) {
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
          this->electric_aux_heating_ = s_safe_(data, length, i + 3) == 1;
          break;
        case 0x1A:
          switch (s_safe_(data, length, i + 3)) {
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
          switch (s_safe_(data, length, i + 3)) {
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
          this->unit_changeable_ = s_safe_(data, length, i + 3) == 0;
          break;
        case 0x24:
          this->light_control_ = s_safe_(data, length, i + 3);
          break;
        case 0x25:
          if (s_safe_(data, length, i + 2) >= 6) {
            this->min_temp_cool_ = static_cast<float>(s_safe_(data, length, i + 3)) * 0.5f;
            this->max_temp_cool_ = static_cast<float>(s_safe_(data, length, i + 4)) * 0.5f;
            this->min_temp_auto_ = static_cast<float>(s_safe_(data, length, i + 5)) * 0.5f;
            this->max_temp_auto_ = static_cast<float>(s_safe_(data, length, i + 6)) * 0.5f;
            this->min_temp_heat_ = static_cast<float>(s_safe_(data, length, i + 7)) * 0.5f;
            this->max_temp_heat_ = static_cast<float>(s_safe_(data, length, i + 8)) * 0.5f;

            if (s_safe_(data, length, i + 2) > 6)
              this->decimals_ = s_safe_(data, length, i + 9) > 0;
            else
              this->decimals_ = s_safe_(data, length, i + 5) != 0;
            break;
            case 0x2C:
              this->buzzer_ = s_safe_(data, length, i + 3) != 0;
              break;
          }
      }
    }
    // Increment cursor and decrement capabilities to process
    i += (3 + s_safe_(data, length, i + 2));
    caps2_process--;
  }

  if (length - i > 1)
    return s_safe_(data, length, length - 2) > 0;

  this->is_ready_ = true;
  return false;
}

const std::string Capabilities::FREEZE_PROTECTION = "freeze protection";
const std::string Capabilities::SILENT = "silent";
const std::string Capabilities::TURBO = "turbo";

void Capabilities::to_climate_traits(ClimateTraits &traits) const {
  if (this->auto_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT_COOL);
  if (this->cool_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  if (this->heat_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  if (this->dry_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);
  if (this->turbo_cool() || this->turbo_heat())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  if (this->eco_mode())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);
  if (this->frost_protection_mode())
    traits.add_supported_custom_preset(FREEZE_PROTECTION);
}

#define LOG_CAPABILITY(tag, str, condition) \
  if (condition) \
    ESP_LOGCONFIG(tag, str);

void Capabilities::dump(const char *tag) const {
  ESP_LOGCONFIG(tag, "CAPABILITIES REPORT:");
  if (this->auto_mode()) {
    ESP_LOGCONFIG(tag, "  [x] AUTO MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_auto());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_auto());
  }
  if (this->cool_mode()) {
    ESP_LOGCONFIG(tag, "  [x] COOL MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_cool());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_cool());
  }
  if (this->heat_mode()) {
    ESP_LOGCONFIG(tag, "  [x] HEAT MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_heat());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_heat());
  }
  LOG_CAPABILITY(tag, "  [x] DRY MODE", this->dry_mode());
  LOG_CAPABILITY(tag, "  [x] ECO MODE", this->eco_mode());
  LOG_CAPABILITY(tag, "  [x] FROST PROTECTION MODE", this->frost_protection_mode());
  LOG_CAPABILITY(tag, "  [x] TURBO COOL", this->turbo_cool());
  LOG_CAPABILITY(tag, "  [x] TURBO HEAT", this->turbo_heat());
  LOG_CAPABILITY(tag, "  [x] FANSPEED CONTROL", this->fanspeed_control());
  LOG_CAPABILITY(tag, "  [x] BREEZE CONTROL", this->breeze_control());
  LOG_CAPABILITY(tag, "  [x] LIGHT CONTROL", this->light_control());
  LOG_CAPABILITY(tag, "  [x] UPDOWN FAN", this->updown_fan());
  LOG_CAPABILITY(tag, "  [x] LEFTRIGHT FAN", this->leftright_fan());
  LOG_CAPABILITY(tag, "  [x] AUTO SET HUMIDITY", this->auto_set_humidity());
  LOG_CAPABILITY(tag, "  [x] MANUAL SET HUMIDITY", this->manual_set_humidity());
  LOG_CAPABILITY(tag, "  [x] INDOOR HUMIDITY", this->indoor_humidity());
  LOG_CAPABILITY(tag, "  [x] POWER CAL", this->power_cal());
  LOG_CAPABILITY(tag, "  [x] POWER CAL SETTING", this->power_cal_setting());
  LOG_CAPABILITY(tag, "  [x] BUZZER", this->buzzer());
  LOG_CAPABILITY(tag, "  [x] SOUND", this->sound());
  LOG_CAPABILITY(tag, "  [x] ACTIVE CLEAN", this->active_clean());
  LOG_CAPABILITY(tag, "  [x] DECIMALS", this->decimals());
  LOG_CAPABILITY(tag, "  [x] ELECTRIC AUX HEATING", this->electric_aux_heating());
  LOG_CAPABILITY(tag, "  [x] NEST CHECK", this->nest_check());
  LOG_CAPABILITY(tag, "  [x] NEST NEED CHANGE", this->nest_need_change());
  LOG_CAPABILITY(tag, "  [x] ONE KEY NO WIND ON ME", this->one_key_no_wind_on_me());
  LOG_CAPABILITY(tag, "  [x] SILKY COOL", this->silky_cool());
  LOG_CAPABILITY(tag, "  [x] SMART EYE", this->smart_eye());
  LOG_CAPABILITY(tag, "  [x] SPECIAL ECO", this->special_eco());
  LOG_CAPABILITY(tag, "  [x] UNIT CHANGEABLE", this->unit_changeable());
  LOG_CAPABILITY(tag, "  [x] WIND OF ME", this->wind_of_me());
  LOG_CAPABILITY(tag, "  [x] WIND ON ME", this->wind_on_me());
}

}  // namespace midea_ac
}  // namespace esphome
