#include "pd_pioneer_data.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome::pd_pioneer_ir {

static const uint8_t DEFAULT_ODD[] = {0x23, 0xCB, 0x26, 0x02, 0x00, 0x40, 0xC0,
                                      0x00, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t DEFAULT_EVEN[] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03,
                                       0x0B, 0x05, 0x00, 0x00, 0x00, 0x80, 0x00};

ControlData::ControlData() {
  std::copy_n(DEFAULT_ODD, PDPioneerData::FRAME_SIZE, this->odd_.data());
  std::copy_n(DEFAULT_EVEN, PDPioneerData::FRAME_SIZE, this->even_.data());
}

void ControlData::finalize() {
  this->odd_.finalize();
  this->even_.finalize();
}

void ControlData::set_power_(bool on) { this->even_[5] = on ? PWR_ON : PWR_OFF; }

bool ControlData::get_power_() const { return this->even_[5] != PWR_OFF; }

void ControlData::set_temp(float temp_c) {
  // Protocol encodes temperature as code = 31.75 - °C.
  // even_[7] = floor(code); even_[12] bit 2 set when fractional part of code is < 0.5.
  temp_c = clamp(temp_c, static_cast<float>(PDPIONEER_TEMPC_MIN), static_cast<float>(PDPIONEER_TEMPC_MAX));
  const float code = 31.75f - temp_c;
  const int whole = static_cast<int>(std::floor(code + 0.001f));
  const float frac = code - static_cast<float>(whole);
  this->even_[7] = static_cast<uint8_t>(whole);
  this->even_[12] = static_cast<uint8_t>(0x80 | ((frac < 0.5f) ? 0x04 : 0x00));
}

float ControlData::get_temp() const {
  if (!this->get_power_())
    return fahrenheit_to_celsius(72.0f);

  const float code = static_cast<float>(this->even_[7]) + ((this->even_[12] & 0x04) ? 0.0f : 0.5f);
  return 31.75f - code;
}

void ControlData::set_mode(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_OFF:
      this->set_power_(false);
      this->even_[6] = MODE_HEAT;
      this->even_[7] = 0x0C;
      this->even_[8] = 0x03;
      this->even_[12] = 0x80;
      this->odd_[5] = 0x40;
      this->odd_[6] = 0x60;
      this->odd_[7] = 0x00;
      return;
    case ClimateMode::CLIMATE_MODE_COOL:
      this->even_[6] = MODE_COOL;
      break;
    case ClimateMode::CLIMATE_MODE_DRY:
      this->even_[6] = MODE_DRY;
      break;
    case ClimateMode::CLIMATE_MODE_FAN_ONLY:
      this->even_[6] = MODE_FAN_ONLY;
      break;
    case ClimateMode::CLIMATE_MODE_HEAT:
      this->even_[6] = MODE_HEAT;
      break;
    default:
      this->even_[6] = MODE_AUTO;
      break;
  }
  this->set_power_(true);
}

ClimateMode ControlData::get_mode() const {
  if (!this->get_power_())
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (this->even_[6]) {
    case MODE_COOL:
      return ClimateMode::CLIMATE_MODE_COOL;
    case MODE_DRY:
      return ClimateMode::CLIMATE_MODE_DRY;
    case MODE_FAN_ONLY:
      return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    case MODE_HEAT:
      return ClimateMode::CLIMATE_MODE_HEAT;
    case MODE_AUTO:
      return ClimateMode::CLIMATE_MODE_HEAT_COOL;
    default:
      return ClimateMode::CLIMATE_MODE_COOL;
  }
}

void ControlData::set_fan_from_odd_(uint8_t byte5, uint8_t byte6) {
  this->odd_[5] = byte5;
  this->odd_[6] = byte6;
}

void ControlData::sync_even_fan_byte_() {
  const uint8_t b6 = this->odd_[6];
  if (b6 == 0x20) {
    this->even_[8] = 0x00;
  } else if (b6 <= 0x40) {
    this->even_[8] = 0x02;
  } else if (b6 <= 0x80) {
    this->even_[8] = 0x03;
  } else {
    this->even_[8] = 0x05;
  }
}

void ControlData::set_fan_mode(ClimateFanMode mode) {
  switch (mode) {
    case ClimateFanMode::CLIMATE_FAN_LOW:
      this->set_fan_from_odd_(0x60, 0x40);
      break;
    case ClimateFanMode::CLIMATE_FAN_MEDIUM:
      this->set_fan_from_odd_(0x40, 0x60);
      break;
    case ClimateFanMode::CLIMATE_FAN_HIGH:
    default:
      this->set_fan_from_odd_(0x40, 0xC0);
      break;
  }
  this->sync_even_fan_byte_();
}

ClimateFanMode ControlData::get_fan_mode() const {
  const uint8_t b6 = this->odd_[6];
  if (b6 == 0x20)
    return ClimateFanMode::CLIMATE_FAN_AUTO;
  if (b6 <= 0x40)
    return ClimateFanMode::CLIMATE_FAN_LOW;
  if (b6 <= 0x80)
    return ClimateFanMode::CLIMATE_FAN_MEDIUM;
  return ClimateFanMode::CLIMATE_FAN_HIGH;
}

void ControlData::set_swing_mode(climate::ClimateSwingMode mode) {
  // Clear prior swing encoding, then apply the requested mode.
  this->odd_[7] = SWING_OFF;
  this->even_[8] = static_cast<uint8_t>(this->even_[8] & ~SWING_VERTICAL_EVEN_MASK);

  switch (mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      this->odd_[7] = SWING_VERTICAL;
      this->even_[8] = static_cast<uint8_t>(this->even_[8] | SWING_VERTICAL_EVEN_MASK);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      this->odd_[7] = SWING_HORIZONTAL;
      break;
    case climate::CLIMATE_SWING_BOTH:
      this->odd_[7] = static_cast<uint8_t>(SWING_VERTICAL | SWING_HORIZONTAL);
      this->even_[8] = static_cast<uint8_t>(this->even_[8] | SWING_VERTICAL_EVEN_MASK);
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      break;
  }
}

climate::ClimateSwingMode ControlData::get_swing_mode() const {
  const uint8_t swing = this->odd_[7];
  if (swing == (SWING_VERTICAL | SWING_HORIZONTAL))
    return climate::CLIMATE_SWING_BOTH;
  if (swing == SWING_VERTICAL)
    return climate::CLIMATE_SWING_VERTICAL;
  if (swing == SWING_HORIZONTAL)
    return climate::CLIMATE_SWING_HORIZONTAL;
  return climate::CLIMATE_SWING_OFF;
}

void ControlData::set_eco(bool enabled) {
  if (enabled) {
    this->even_[5] = 0x25;
  } else if (this->even_[5] == 0x25) {
    this->even_[5] = PWR_ON;
  }
}

bool ControlData::get_eco() const { return this->even_[5] == 0x25; }

void ControlData::apply_odd(const PDPioneerData &data) {
  for (uint8_t i = 0; i < PDPioneerData::DATA_LEN; i++)
    this->odd_[i] = data[i];
}

void ControlData::apply_even(const PDPioneerData &data) {
  for (uint8_t i = 0; i < PDPioneerData::DATA_LEN; i++)
    this->even_[i] = data[i];
}

}  // namespace esphome::pd_pioneer_ir
