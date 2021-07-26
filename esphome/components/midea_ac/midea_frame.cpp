#include "midea_frame.h"
#include "capabilities.h"

namespace esphome {
namespace midea_ac {

static const char *const TAG = "midea_ac";

const uint8_t CommandFrame::INIT[] = {0xAA, 0x23, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00,
                                      0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

float PropertiesFrame::get_target_temp() const {
  float temp = static_cast<float>(this->get_val_(12, 0, 0b1111) + 16);
  if (this->get_val_(12, 0, 0x10))
    temp += 0.5;
  return temp;
}

void PropertiesFrame::set_target_temp(float temp) {
  uint8_t tmp = static_cast<uint8_t>(temp * 16.0) + 4;
  tmp = ((tmp & 8) << 1) | (tmp >> 4);
  this->set_val_(12, 0, 0x1F, tmp);
}

static float i16tof(int16_t in) { return static_cast<float>(in - 50) * 0.5f; }
float PropertiesFrame::get_indoor_temp() const { return i16tof(this->get_val_(21)); }
float PropertiesFrame::get_outdoor_temp() const { return i16tof(this->get_val_(22)); }
float PropertiesFrame::get_humidity_setpoint() const { return static_cast<float>(this->get_val_(29, 0, 0x7F)); }

ClimateMode PropertiesFrame::get_mode() const {
  if (!this->get_power_())
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (this->get_val_(12, 5)) {
    case MideaMode::MIDEA_MODE_AUTO:
      return ClimateMode::CLIMATE_MODE_HEAT_COOL;
    case MideaMode::MIDEA_MODE_COOL:
      return ClimateMode::CLIMATE_MODE_COOL;
    case MideaMode::MIDEA_MODE_DRY:
      return ClimateMode::CLIMATE_MODE_DRY;
    case MideaMode::MIDEA_MODE_HEAT:
      return ClimateMode::CLIMATE_MODE_HEAT;
    case MideaMode::MIDEA_MODE_FAN_ONLY:
      return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    default:
      return ClimateMode::CLIMATE_MODE_OFF;
  }
}

void PropertiesFrame::set_mode(ClimateMode mode) {
  uint8_t m;
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_HEAT_COOL:
      m = MideaMode::MIDEA_MODE_AUTO;
      break;
    case ClimateMode::CLIMATE_MODE_COOL:
      m = MideaMode::MIDEA_MODE_COOL;
      break;
    case ClimateMode::CLIMATE_MODE_DRY:
      m = MideaMode::MIDEA_MODE_DRY;
      break;
    case ClimateMode::CLIMATE_MODE_HEAT:
      m = MideaMode::MIDEA_MODE_HEAT;
      break;
    case ClimateMode::CLIMATE_MODE_FAN_ONLY:
      m = MideaMode::MIDEA_MODE_FAN_ONLY;
      break;
    default:
      this->set_power_(false);
      return;
  }
  this->set_power_(true);
  this->set_val_(12, 5, 0b111, m);
}

optional<ClimatePreset> PropertiesFrame::get_preset() const {
  if (this->get_eco_mode())
    return ClimatePreset::CLIMATE_PRESET_ECO;
  if (this->get_sleep_mode())
    return ClimatePreset::CLIMATE_PRESET_SLEEP;
  if (this->get_turbo_mode())
    return ClimatePreset::CLIMATE_PRESET_BOOST;
  return ClimatePreset::CLIMATE_PRESET_NONE;
}

void PropertiesFrame::set_preset(ClimatePreset preset) {
  this->clear_presets();
  switch (preset) {
    case ClimatePreset::CLIMATE_PRESET_ECO:
      this->set_eco_mode(true);
      break;
    case ClimatePreset::CLIMATE_PRESET_SLEEP:
      this->set_sleep_mode(true);
      break;
    case ClimatePreset::CLIMATE_PRESET_BOOST:
      this->set_turbo_mode(true);
      break;
    default:
      break;
  }
}

void PropertiesFrame::clear_presets() {
  this->set_eco_mode(false);
  this->set_sleep_mode(false);
  this->set_turbo_mode(false);
  this->set_freeze_protection_mode(false);
}

bool PropertiesFrame::is_custom_preset() const { return this->get_freeze_protection_mode(); }

const std::string &PropertiesFrame::get_custom_preset() const { return Capabilities::FREEZE_PROTECTION; };

void PropertiesFrame::set_custom_preset(const std::string &preset) {
  this->clear_presets();
  if (preset == Capabilities::FREEZE_PROTECTION)
    this->set_freeze_protection_mode(true);
}

bool PropertiesFrame::is_custom_fan_mode() const {
  switch (this->pb_[13]) {
    case MideaFanMode::MIDEA_FAN_SILENT:
    case MideaFanMode::MIDEA_FAN_TURBO:
      return true;
    default:
      return false;
  }
}

ClimateFanMode PropertiesFrame::get_fan_mode() const {
  switch (this->pb_[13]) {
    case MideaFanMode::MIDEA_FAN_LOW:
      return ClimateFanMode::CLIMATE_FAN_LOW;
    case MideaFanMode::MIDEA_FAN_MEDIUM:
      return ClimateFanMode::CLIMATE_FAN_MEDIUM;
    case MideaFanMode::MIDEA_FAN_HIGH:
      return ClimateFanMode::CLIMATE_FAN_HIGH;
    default:
      return ClimateFanMode::CLIMATE_FAN_AUTO;
  }
}

void PropertiesFrame::set_fan_mode(ClimateFanMode mode) {
  uint8_t m;
  switch (mode) {
    case ClimateFanMode::CLIMATE_FAN_LOW:
      m = MideaFanMode::MIDEA_FAN_LOW;
      break;
    case ClimateFanMode::CLIMATE_FAN_MEDIUM:
      m = MideaFanMode::MIDEA_FAN_MEDIUM;
      break;
    case ClimateFanMode::CLIMATE_FAN_HIGH:
      m = MideaFanMode::MIDEA_FAN_HIGH;
      break;
    default:
      m = MideaFanMode::MIDEA_FAN_AUTO;
      break;
  }
  this->pb_[13] = m;
}

const std::string &PropertiesFrame::get_custom_fan_mode() const {
  switch (this->pb_[13]) {
    case MideaFanMode::MIDEA_FAN_SILENT:
      return Capabilities::SILENT;
    default:
      return Capabilities::TURBO;
  }
}

void PropertiesFrame::set_custom_fan_mode(const std::string &mode) {
  uint8_t m;
  if (mode == Capabilities::SILENT) {
    m = MideaFanMode::MIDEA_FAN_SILENT;
  } else {
    m = MideaFanMode::MIDEA_FAN_TURBO;
  }
  this->pb_[13] = m;
}

ClimateSwingMode PropertiesFrame::get_swing_mode() const {
  switch (this->get_val_(17, 0, 0b1111)) {
    case MideaSwingMode::MIDEA_SWING_VERTICAL:
      return ClimateSwingMode::CLIMATE_SWING_VERTICAL;
    case MideaSwingMode::MIDEA_SWING_HORIZONTAL:
      return ClimateSwingMode::CLIMATE_SWING_HORIZONTAL;
    case MideaSwingMode::MIDEA_SWING_BOTH:
      return ClimateSwingMode::CLIMATE_SWING_BOTH;
    default:
      return ClimateSwingMode::CLIMATE_SWING_OFF;
  }
}

void PropertiesFrame::set_swing_mode(ClimateSwingMode mode) {
  uint8_t m;
  switch (mode) {
    case ClimateSwingMode::CLIMATE_SWING_VERTICAL:
      m = MideaSwingMode::MIDEA_SWING_VERTICAL;
      break;
    case ClimateSwingMode::CLIMATE_SWING_HORIZONTAL:
      m = MideaSwingMode::MIDEA_SWING_HORIZONTAL;
      break;
    case ClimateSwingMode::CLIMATE_SWING_BOTH:
      m = MideaSwingMode::MIDEA_SWING_BOTH;
      break;
    default:
      m = MideaSwingMode::MIDEA_SWING_OFF;
      break;
  }
  this->pb_[17] = 0x30 | m;
}

float PropertiesFrame::get_power_usage() const {
  uint32_t power = 0;
  const uint8_t *ptr = this->pb_ + 28;
  for (uint32_t weight = 1;; weight *= 10, ptr--) {
    power += (*ptr % 16) * weight;
    weight *= 10;
    power += (*ptr / 16) * weight;
    if (weight == 100000)
      return static_cast<float>(power) * 0.1f;
  }
}

}  // namespace midea_ac
}  // namespace esphome
