#include "midea_frame.h"

namespace esphome {
namespace midea_ac {

const uint8_t QueryFrame::INIT[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x41, 0x00,
                                    0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x68};

const uint8_t CommandFrame::INIT[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x40, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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

static float i8tof(int8_t in) {
  const bool half = in & 1;
  in = (in - 50) / 2;
  float out = static_cast<float>(in);
  if (half)
    out += 0.5;
  return out;
}

float PropertiesFrame::get_indoor_temp() const { return i8tof(this->pbuf_[21]); }

float PropertiesFrame::get_outdoor_temp() const { return i8tof(this->pbuf_[22]); }

climate::ClimateMode PropertiesFrame::get_mode() const {
  if (!this->get_power_())
    return climate::CLIMATE_MODE_OFF;
  switch (this->pbuf_[12] >> 5) {
    case 1:
      return climate::CLIMATE_MODE_AUTO;
    case 2:
      return climate::CLIMATE_MODE_COOL;
    case 3:
      return climate::CLIMATE_MODE_DRY;
    case 4:
      return climate::CLIMATE_MODE_HEAT;
    case 5:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_OFF;
  }
}

void PropertiesFrame::set_mode(climate::ClimateMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_MODE_AUTO:
      m = (1 << 5);
      break;
    case climate::CLIMATE_MODE_COOL:
      m = (2 << 5);
      break;
    case climate::CLIMATE_MODE_DRY:
      m = (3 << 5);
      break;
    case climate::CLIMATE_MODE_HEAT:
      m = (4 << 5);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      m = (5 << 5);
      break;
    default:
      this->set_power_(false);
      return;
  }
  this->set_power_(true);
  this->pbuf_[12] &= ~0xE0;
  this->pbuf_[12] |= m;
}

climate::ClimateFanMode PropertiesFrame::get_fan_mode() const {
  switch (this->pbuf_[13]) {
    case 40:
      return climate::CLIMATE_FAN_LOW;
    case 60:
      return climate::CLIMATE_FAN_MEDIUM;
    case 80:
      return climate::CLIMATE_FAN_HIGH;
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

void PropertiesFrame::set_fan_mode(climate::ClimateFanMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_FAN_LOW:
      m = 40;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      m = 60;
      break;
    case climate::CLIMATE_FAN_HIGH:
      m = 80;
      break;
    default:
      m = 102;
      break;
  }
  this->pbuf_[13] = m;
}

climate::ClimateSwingMode PropertiesFrame::get_swing_mode() const {
  switch (this->pbuf_[17] & 0x0F) {
    case 0b1100:
      return climate::CLIMATE_SWING_VERTICAL;
    case 0b0011:
      return climate::CLIMATE_SWING_HORIZONTAL;
    case 0b1111:
      return climate::CLIMATE_SWING_BOTH;
    default:
      return climate::CLIMATE_SWING_OFF;
  }
}

void PropertiesFrame::set_swing_mode(climate::ClimateSwingMode mode) {
  uint8_t m;
  switch (mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      m = 0x30 | 0b1100;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      m = 0x30 | 0b0011;
      break;
    case climate::CLIMATE_SWING_BOTH:
      m = 0x30 | 0b1111;
      break;
    default:
      m = 0x30;
      break;
  }
  this->pbuf_[17] = m;
}

}  // namespace midea_ac
}  // namespace esphome
