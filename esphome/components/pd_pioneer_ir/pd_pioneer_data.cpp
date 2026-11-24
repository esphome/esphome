#include "pd_pioneer_data.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::pd_pioneer_ir {

// Default frames stay in flash; the constructor copies them with progmem_memcpy. Even byte 5 is PWR_ON, byte 6
// MODE_COOL
static const uint8_t DEFAULT_ODD[] PROGMEM = {0x23, 0xCB, 0x26, 0x02, 0x00, 0x40, 0xC0,
                                              0x00, 0xC3, 0x00, 0x00, 0x00, 0x00};
static const uint8_t DEFAULT_EVEN[] PROGMEM = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03,
                                               0x0B, 0x05, 0x00, 0x00, 0x00, 0x80};

// Odd positions 5 and 6 and the even position 8 fan code, indexed from CLIMATE_FAN_AUTO to CLIMATE_FAN_HIGH
static const uint8_t FAN_CODES[][3] PROGMEM = {
    {0x40, 0x20, 0x00}, {0x60, 0x40, 0x02}, {0x40, 0x60, 0x03}, {0x40, 0xC0, 0x05}};
static constexpr size_t FAN_CODE_COUNT = sizeof(FAN_CODES) / sizeof(FAN_CODES[0]);

ControlData::ControlData() : odd_(DEFAULT_ODD, sizeof(DEFAULT_ODD)), even_(DEFAULT_EVEN, sizeof(DEFAULT_EVEN)) {}

void ControlData::set_temp(float temp_c) {
  // The temperature travels as 31 - C in whole degrees at position 7, with bit 2 of position 12 adding a half
  temp_c = clamp(temp_c, static_cast<float>(PDPIONEER_TEMPC_MIN), static_cast<float>(PDPIONEER_TEMPC_MAX));
  // Round to a quarter, then down to a half, so Fahrenheit set points encode as before
  const int halves = static_cast<int>(lroundf(temp_c * 4)) / 2;
  this->even_[7] = static_cast<uint8_t>(31 - halves / 2);
  this->even_[12] = static_cast<uint8_t>(0x80 | ((halves & 1) ? 0x04 : 0x00));
}

void ControlData::set_mode(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_OFF:
      this->even_[5] = PWR_OFF;
      this->even_[6] = MODE_HEAT;
      this->even_[7] = 0x0C;
      this->even_[12] = 0x80;
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
  this->even_[5] = PWR_ON;
}

optional<ClimateMode> ControlData::get_mode(const PDPioneerData &even) {
  if (even[5] == PWR_OFF)
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (even[6]) {
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
      return {};
  }
}

void ControlData::finalize() {
  const size_t idx = this->fan_ >= climate::CLIMATE_FAN_AUTO && this->fan_ <= climate::CLIMATE_FAN_HIGH
                         ? this->fan_ - climate::CLIMATE_FAN_AUTO
                         : FAN_CODE_COUNT - 1;
  this->odd_[5] = progmem_read_byte(&FAN_CODES[idx][0]);
  this->odd_[6] = progmem_read_byte(&FAN_CODES[idx][1]);
  this->odd_[7] = this->swing_;
  this->even_[8] = static_cast<uint8_t>(progmem_read_byte(&FAN_CODES[idx][2]) |
                                        ((this->swing_ & SWING_VERTICAL) ? SWING_VERTICAL_EVEN_MASK : 0));
  this->odd_.finalize();
  this->even_.finalize();
}

ClimateFanMode ControlData::get_fan_mode(const PDPioneerData &even) {
  const uint8_t code = even[8] & ~SWING_VERTICAL_EVEN_MASK;
  for (size_t idx = 0; idx < FAN_CODE_COUNT; idx++) {
    if (progmem_read_byte(&FAN_CODES[idx][2]) == code)
      return static_cast<ClimateFanMode>(climate::CLIMATE_FAN_AUTO + idx);
  }
  return ClimateFanMode::CLIMATE_FAN_HIGH;
}

void ControlData::set_swing_mode(climate::ClimateSwingMode mode) {
  const bool vertical = mode == climate::CLIMATE_SWING_VERTICAL || mode == climate::CLIMATE_SWING_BOTH;
  const bool horizontal = mode == climate::CLIMATE_SWING_HORIZONTAL || mode == climate::CLIMATE_SWING_BOTH;
  this->swing_ = static_cast<uint8_t>((vertical ? SWING_VERTICAL : 0) | (horizontal ? SWING_HORIZONTAL : 0));
}

climate::ClimateSwingMode ControlData::get_swing_mode(const PDPioneerData &even, bool horizontal) {
  const bool vertical = (even[8] & SWING_VERTICAL_EVEN_MASK) == SWING_VERTICAL_EVEN_MASK;
  if (vertical)
    return horizontal ? climate::CLIMATE_SWING_BOTH : climate::CLIMATE_SWING_VERTICAL;
  return horizontal ? climate::CLIMATE_SWING_HORIZONTAL : climate::CLIMATE_SWING_OFF;
}

}  // namespace esphome::pd_pioneer_ir
