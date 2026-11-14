#include "whynter.h"
#include "esphome/core/log.h"

namespace esphome::whynter {

ESPHOME_LOG_TAG(TAG, "climate.whynter");

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

using climate_ir::find_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// Codes are stored shifted down to one byte. A mode not listed is sent as power off, and ignored when received.
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_FAN_ONLY, MODE_FAN >> MODE_SHIFT},
    {climate::CLIMATE_MODE_DRY, MODE_DRY >> MODE_SHIFT},
    {climate::CLIMATE_MODE_HEAT, MODE_HEAT >> MODE_SHIFT},
    {climate::CLIMATE_MODE_COOL, MODE_COOL >> MODE_SHIFT},
};
// A received code that is not listed is ignored
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_HIGH, FAN_HIGH >> FAN_SHIFT},
    {climate::CLIMATE_FAN_LOW, FAN_LOW >> FAN_SHIFT},
    {climate::CLIMATE_FAN_MEDIUM, FAN_MED >> FAN_SHIFT},
};

void Whynter::transmit_state() {
  uint32_t remote_state = COMMAND_CODE;
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL)
    this->mode = climate::CLIMATE_MODE_COOL;
  if (const auto entry = find_ir_code(MODES, this->mode, false)) {
    remote_state |= POWER_MASK;
    remote_state |= static_cast<uint32_t>(entry->code) << MODE_SHIFT;
  } else {
    remote_state |= POWER_OFF;
  }
  mode_before_ = this->mode;

  remote_state |=
      static_cast<uint32_t>(to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_HIGH), FAN_HIGH >> FAN_SHIFT))
      << FAN_SHIFT;

  if (fahrenheit_) {
    remote_state |= UNIT_MASK;
    uint8_t temp = (uint8_t) roundf(
        clamp<float>(esphome::celsius_to_fahrenheit(this->target_temperature), TEMP_MIN_F, TEMP_MAX_F));
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

  ESP_LOGD(TAG, "Decoded 0x%02" PRIX32, remote_state);
  if ((remote_state & COMMAND_MASK) != COMMAND_CODE)
    return false;
  if ((remote_state & POWER_MASK) != POWER_MASK) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    this->mode = from_ir_code(MODES, (remote_state & MODE_MASK) >> MODE_SHIFT, this->mode);

    // Temperature
    if ((remote_state & UNIT_MASK) == UNIT_MASK) {  // Fahrenheit
      this->target_temperature = esphome::fahrenheit_to_celsius(esphome::reverse_bits(remote_state & TEMP_MASK) >> 24);
    } else {  // Celsius
      this->target_temperature = (esphome::reverse_bits(remote_state & TEMP_MASK) >> 24) + TEMP_OFFSET_C;
    }

    // Fan Speed
    if (const auto entry = find_ir_code(FANS, (remote_state & FAN_MASK) >> FAN_SHIFT, true))
      this->fan_mode = static_cast<climate::ClimateFanMode>(entry->value);
  }
  this->publish_state();

  return true;
}

void Whynter::transmit_(uint32_t value) {
  ESP_LOGD(TAG, "Sending Whynter code: 0x%02" PRIX32, value);

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

}  // namespace esphome::whynter
