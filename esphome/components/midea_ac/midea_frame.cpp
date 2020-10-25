#include "midea_frame.h"

namespace esphome {
namespace midea_ac {

const uint8_t QueryFrame::INIT[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x41, 0x00,
                                    0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x68};

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

static float i8tof(int8_t in) { return static_cast<float>(in - 50) / 2.0; }
float PropertiesFrame::get_indoor_temp() const { return i8tof(this->pbuf_[21]); }
float PropertiesFrame::get_outdoor_temp() const { return i8tof(this->pbuf_[22]); }

climate::ClimateMode PropertiesFrame::get_mode() const {
  if (!this->get_power_())
    return climate::CLIMATE_MODE_OFF;
  switch (this->pbuf_[12] >> 5) {
    case MIDEA_MODE_AUTO:
      return climate::CLIMATE_MODE_AUTO;
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
    case climate::CLIMATE_MODE_AUTO:
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

}  // namespace midea_ac
}  // namespace esphome
