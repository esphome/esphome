#include "midea_frame.h"

namespace esphome {
namespace midea_ac {

static const char *TAG = "midea_ac";
const std::string MIDEA_SILENT_FAN_MODE = "silent";
const std::string MIDEA_TURBO_FAN_MODE = "turbo";
const std::string MIDEA_FREEZE_PROTECTION_PRESET = "freeze protection";

const uint8_t QueryFrame::INIT[] = {0xAA, 0x21, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x41, 0x81,
                                    0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x37, 0x31};

const uint8_t PowerQueryFrame::INIT[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x41, 0x21,
                                         0x01, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x17, 0x6A};

const uint8_t CommandFrame::INIT[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x40, 0x00,
                                      0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

float PropertiesFrame::get_target_temp() const {
  float temp = static_cast<float>((this->pbuf_[12] & 0x0F) + 16);
  if (this->pbuf_[12] & 0x10)
    temp += 0.5;
  return temp;
}

void PropertiesFrame::set_target_temp(float temp) {
  uint8_t tmp = static_cast<uint8_t>(temp * 16.0) + 4;
  tmp = ((tmp & 8) << 1) | (tmp >> 4);
  this->pbuf_[12] &= ~0x1F;
  this->pbuf_[12] |= tmp;
}

static float i16tof(int16_t in) { return static_cast<float>(in - 50) / 2.0; }
float PropertiesFrame::get_indoor_temp() const { return i16tof(this->pbuf_[21]); }
float PropertiesFrame::get_outdoor_temp() const { return i16tof(this->pbuf_[22]); }
float PropertiesFrame::get_humidity_setpoint() const { return static_cast<float>(this->pbuf_[29] & 0x7F); }

climate::ClimateMode PropertiesFrame::get_mode() const {
  if (!this->get_power_())
    return climate::CLIMATE_MODE_OFF;
  switch (this->pbuf_[12] >> 5) {
    case MIDEA_MODE_AUTO:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case MIDEA_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case MIDEA_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case MIDEA_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case MIDEA_MODE_FAN_ONLY:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_OFF;
  }
}

void PropertiesFrame::set_mode(climate::ClimateMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      m = MIDEA_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      m = MIDEA_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      m = MIDEA_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      m = MIDEA_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      m = MIDEA_MODE_FAN_ONLY;
      break;
    default:
      this->set_power_(false);
      return;
  }
  this->set_power_(true);
  this->pbuf_[12] &= ~0xE0;
  this->pbuf_[12] |= m << 5;
}

optional<climate::ClimatePreset> PropertiesFrame::get_preset() const {
  if (this->get_eco_mode()) {
    return climate::CLIMATE_PRESET_ECO;
  } else if (this->get_sleep_mode()) {
    return climate::CLIMATE_PRESET_SLEEP;
  } else if (this->get_turbo_mode()) {
    return climate::CLIMATE_PRESET_BOOST;
  } else {
    return climate::CLIMATE_PRESET_HOME;
  }
}

void PropertiesFrame::set_preset(climate::ClimatePreset preset) {
  switch (preset) {
    case climate::CLIMATE_PRESET_ECO:
      this->set_eco_mode(true);
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      this->set_sleep_mode(true);
      break;
    case climate::CLIMATE_PRESET_BOOST:
      this->set_turbo_mode(true);
      break;
    default:
      break;
  }
}

bool PropertiesFrame::is_custom_preset() const { return this->get_freeze_protection_mode(); }

const std::string &PropertiesFrame::get_custom_preset() const { return midea_ac::MIDEA_FREEZE_PROTECTION_PRESET; };

void PropertiesFrame::set_custom_preset(const std::string &preset) {
  if (preset == MIDEA_FREEZE_PROTECTION_PRESET) {
    this->set_freeze_protection_mode(true);
  }
}

bool PropertiesFrame::is_custom_fan_mode() const {
  switch (this->pbuf_[13]) {
    case MIDEA_FAN_SILENT:
    case MIDEA_FAN_TURBO:
      return true;
    default:
      return false;
  }
}

climate::ClimateFanMode PropertiesFrame::get_fan_mode() const {
  switch (this->pbuf_[13]) {
    case MIDEA_FAN_LOW:
      return climate::CLIMATE_FAN_LOW;
    case MIDEA_FAN_MEDIUM:
      return climate::CLIMATE_FAN_MEDIUM;
    case MIDEA_FAN_HIGH:
      return climate::CLIMATE_FAN_HIGH;
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

void PropertiesFrame::set_fan_mode(climate::ClimateFanMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_FAN_LOW:
      m = MIDEA_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      m = MIDEA_FAN_MEDIUM;
      break;
    case climate::CLIMATE_FAN_HIGH:
      m = MIDEA_FAN_HIGH;
      break;
    default:
      m = MIDEA_FAN_AUTO;
      break;
  }
  this->pbuf_[13] = m;
}

const std::string &PropertiesFrame::get_custom_fan_mode() const {
  switch (this->pbuf_[13]) {
    case MIDEA_FAN_SILENT:
      return MIDEA_SILENT_FAN_MODE;
    default:
      return MIDEA_TURBO_FAN_MODE;
  }
}

void PropertiesFrame::set_custom_fan_mode(const std::string &mode) {
  uint8_t m;
  if (mode == MIDEA_SILENT_FAN_MODE) {
    m = MIDEA_FAN_SILENT;
  } else {
    m = MIDEA_FAN_TURBO;
  }
  this->pbuf_[13] = m;
}

climate::ClimateSwingMode PropertiesFrame::get_swing_mode() const {
  switch (this->pbuf_[17] & 0x0F) {
    case MIDEA_SWING_VERTICAL:
      return climate::CLIMATE_SWING_VERTICAL;
    case MIDEA_SWING_HORIZONTAL:
      return climate::CLIMATE_SWING_HORIZONTAL;
    case MIDEA_SWING_BOTH:
      return climate::CLIMATE_SWING_BOTH;
    default:
      return climate::CLIMATE_SWING_OFF;
  }
}

void PropertiesFrame::set_swing_mode(climate::ClimateSwingMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      m = MIDEA_SWING_VERTICAL;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      m = MIDEA_SWING_HORIZONTAL;
      break;
    case climate::CLIMATE_SWING_BOTH:
      m = MIDEA_SWING_BOTH;
      break;
    default:
      m = MIDEA_SWING_OFF;
      break;
  }
  this->pbuf_[17] = 0x30 | m;
}

float PropertiesFrame::get_power_usage() const {
  uint32_t power = 0;
  const uint8_t *ptr = this->pbuf_ + 28;
  for (uint32_t weight = 1;; weight *= 10, ptr--) {
    power += (*ptr % 16) * weight;
    weight *= 10;
    power += (*ptr / 16) * weight;
    if (weight == 100000)
      return static_cast<float>(power) * 0.1;
  }
}

}  // namespace midea_ac
}  // namespace esphome
