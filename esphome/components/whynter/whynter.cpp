#include "whynter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace whynter {

static const char *const TAG = "climate.whynter";

const uint16_t BITS = 32;

// Static First Byte
const uint32_t COMMAND_MASK = 0xFF << 24;
const uint32_t COMMAND_CODE = 0x12 << 24;

// Power
const uint32_t POWER_SHIFT = 8;
const uint32_t POWER_MASK = 1 << POWER_SHIFT;
const uint32_t POWER_OFF = 0 << POWER_SHIFT;

// Mode
const uint32_t MODE_SHIFT = 16;
const uint32_t MODE_MASK = 0b1111 << MODE_SHIFT;
const uint32_t MODE_FAN = 0b0001 << MODE_SHIFT;
const uint32_t MODE_DRY = 0b0010 << MODE_SHIFT;
const uint32_t MODE_HEAT = 0b0100 << MODE_SHIFT;
const uint32_t MODE_COOL = 0b1000 << MODE_SHIFT;

// Fan Speed
const uint32_t FAN_SHIFT = 20;
const uint32_t FAN_MASK = 0b111 << FAN_SHIFT;
const uint32_t FAN_HIGH = 0b001 << FAN_SHIFT;
const uint32_t FAN_MED = 0b010 << FAN_SHIFT;
const uint32_t FAN_LOW = 0b100 << FAN_SHIFT;

// Temperature Unit
const uint32_t UNIT_SHIFT = 10;
const uint32_t UNIT_MASK = 1 << UNIT_SHIFT;

// Temperature Value
const uint32_t TEMP_MASK = 0xFF;
const uint32_t TEMP_OFFSET_C = 16;

void Whynter::transmit_state() {
  uint32_t remote_state = COMMAND_CODE;
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL)
    this->mode = climate::CLIMATE_MODE_COOL;
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state |= POWER_MASK;
      remote_state |= MODE_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state |= POWER_MASK;
      remote_state |= MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state |= POWER_MASK;
      remote_state |= MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state |= POWER_MASK;
      remote_state |= MODE_COOL;
      break;
    case climate::CLIMATE_MODE_OFF:
      remote_state |= POWER_OFF;
      break;
    default:
      remote_state |= POWER_OFF;
  }
  mode_before_ = this->mode;

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      remote_state |= FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state |= FAN_MED;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state |= FAN_HIGH;
      break;
    default:
      remote_state |= FAN_HIGH;
  }

  if (fahrenheit_) {
    remote_state |= UNIT_MASK;
    uint8_t temp =
        (uint8_t) clamp<float>(esphome::celsius_to_fahrenheit(this->target_temperature), TEMP_MIN_F, TEMP_MAX_F);
    temp = esphome::reverse_bits(temp);
    remote_state |= temp;
  } else {
    uint8_t temp = (uint8_t) roundf(clamp<float>(this->target_temperature, TEMP_MIN_C, TEMP_MAX_C) - TEMP_OFFSET_C);
    temp = esphome::reverse_bits(temp);
    remote_state |= temp;
  }

  transmit_(remote_state);
  this->publish_state();
}

bool Whynter::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t nbits = 0;
  uint32_t remote_state = 0;

  if (!data.expect_item(this->header_high_, this->header_low_))
    return false;

  for (nbits = 0; nbits < 32; nbits++) {
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      remote_state = (remote_state << 1) | 1;
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      remote_state = (remote_state << 1) | 0;
    } else if (nbits == BITS) {
      break;
    } else {
      return false;
    }
  }

  ESP_LOGD(TAG, "Decoded 0x%02X", remote_state);
  if ((remote_state & COMMAND_MASK) != COMMAND_CODE)
    return false;
  if ((remote_state & POWER_MASK) != POWER_MASK) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    if ((remote_state & MODE_MASK) == MODE_FAN) {
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    } else if ((remote_state & MODE_MASK) == MODE_DRY) {
      this->mode = climate::CLIMATE_MODE_DRY;
    } else if ((remote_state & MODE_MASK) == MODE_HEAT) {
      this->mode = climate::CLIMATE_MODE_HEAT;
    } else if ((remote_state & MODE_MASK) == MODE_COOL) {
      this->mode = climate::CLIMATE_MODE_COOL;
    }

    // Temperature
    if ((remote_state & UNIT_MASK) == UNIT_MASK) {  // Fahrenheit
      this->target_temperature = esphome::fahrenheit_to_celsius(esphome::reverse_bits(remote_state & TEMP_MASK) >> 24);
    } else {  // Celsius
      this->target_temperature = (esphome::reverse_bits(remote_state & TEMP_MASK) >> 24) + TEMP_OFFSET_C;
    }

    // Fan Speed
    if ((remote_state & FAN_MASK) == FAN_LOW) {
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    } else if ((remote_state & FAN_MASK) == FAN_MED) {
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    } else if ((remote_state & FAN_MASK) == FAN_HIGH) {
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
    }
  }
  this->publish_state();

  return true;
}

void Whynter::transmit_(uint32_t value) {
  ESP_LOGD(TAG, "Sending whynter code: 0x%02X", value);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  data->reserve(2 + BITS * 2u);

  data->item(this->header_high_, this->header_low_);

  for (uint32_t mask = 1UL << (BITS - 1); mask != 0; mask >>= 1) {
    if (value & mask) {
      data->item(this->bit_high_, this->bit_one_low_);
    } else {
      data->item(this->bit_high_, this->bit_zero_low_);
    }
  }
  data->mark(this->bit_high_);
  transmit.perform();
}

}  // namespace whynter
}  // namespace esphome
