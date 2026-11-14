#include "tcl112.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tcl112 {

ESPHOME_LOG_TAG(TAG, "tcl112.climate");

const uint16_t TCL112_STATE_LENGTH = 14;
const uint16_t TCL112_BITS = TCL112_STATE_LENGTH * 8;

const uint8_t TCL112_HEAT = 1;
const uint8_t TCL112_DRY = 2;
const uint8_t TCL112_COOL = 3;
const uint8_t TCL112_FAN = 7;
const uint8_t TCL112_AUTO = 8;

const uint8_t TCL112_FAN_AUTO = 0;
const uint8_t TCL112_FAN_LOW = 2;
const uint8_t TCL112_FAN_MED = 3;
const uint8_t TCL112_FAN_HIGH = 5;

const uint8_t TCL112_VSWING_MASK = 0x38;
const uint8_t TCL112_POWER_MASK = 0x04;

const uint8_t TCL112_HALF_DEGREE = 0b00100000;

const uint16_t TCL112_HEADER_MARK = 3100;
const uint16_t TCL112_HEADER_SPACE = 1650;
const uint16_t TCL112_BIT_MARK = 500;
static constexpr remote_base::BitTimings TCL112_BIT_TIMINGS{
    .bit_mark = TCL112_BIT_MARK, .one_space = 1100, .zero_space = 350};
const uint32_t TCL112_GAP = TCL112_HEADER_SPACE;

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// A mode not listed is sent as power off, and ignored when received
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, TCL112_AUTO}, {climate::CLIMATE_MODE_COOL, TCL112_COOL},
    {climate::CLIMATE_MODE_HEAT, TCL112_HEAT},      {climate::CLIMATE_MODE_DRY, TCL112_DRY},
    {climate::CLIMATE_MODE_FAN_ONLY, TCL112_FAN},
};
// The first entry is the fallback
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, TCL112_FAN_AUTO},
    {climate::CLIMATE_FAN_HIGH, TCL112_FAN_HIGH},
    {climate::CLIMATE_FAN_MEDIUM, TCL112_FAN_MED},
    {climate::CLIMATE_FAN_LOW, TCL112_FAN_LOW},
};

void Tcl112Climate::transmit_state() {
  uint8_t remote_state[TCL112_STATE_LENGTH] = {0};

  // A known good state. (On, Cool, 24C)
  remote_state[0] = 0x23;
  remote_state[1] = 0xCB;
  remote_state[2] = 0x26;
  remote_state[3] = 0x01;
  remote_state[5] = 0x24;
  remote_state[6] = 0x03;
  remote_state[7] = 0x07;
  remote_state[8] = 0x40;

  // Set mode
  if (const auto entry = find_ir_code(MODES, this->mode, false)) {
    remote_state[6] &= 0xF0;
    remote_state[6] |= entry->code;
  } else {
    remote_state[5] &= ~TCL112_POWER_MASK;
  }

  // Set temperature
  // Make sure we have desired temp in the correct range.
  float safecelsius = std::max(this->target_temperature, TCL112_TEMP_MIN);
  safecelsius = std::min(safecelsius, TCL112_TEMP_MAX);
  // Convert to integer nr. of half degrees.
  auto half_degrees = static_cast<uint8_t>(safecelsius * 2);
  if (half_degrees & 1) {                    // Do we have a half degree celsius?
    remote_state[12] |= TCL112_HALF_DEGREE;  // Add 0.5 degrees
  } else {
    remote_state[12] &= ~TCL112_HALF_DEGREE;  // Clear the half degree.
  }
  remote_state[7] &= 0xF0;  // Clear temp bits.
  remote_state[7] |= ((uint8_t) TCL112_TEMP_MAX - half_degrees / 2);

  // Set fan
  remote_state[8] |= to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));

  // Set swing
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    remote_state[8] |= TCL112_VSWING_MASK;
  }

  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  for (uint8_t checksum_byte = 0; checksum_byte < TCL112_STATE_LENGTH - 1; checksum_byte++)
    remote_state[TCL112_STATE_LENGTH - 1] += remote_state[checksum_byte];

  ESP_LOGV_HEX(TAG, remote_state, TCL112_STATE_LENGTH, TCL112_STATE_LENGTH, "Sending: %s");

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(TCL112_HEADER_MARK);
  data->space(TCL112_HEADER_SPACE);
  // Data
  data->item_bytes<TCL112_BIT_TIMINGS>(remote_state, TCL112_STATE_LENGTH);
  // Footer
  data->mark(TCL112_BIT_MARK);
  data->space(TCL112_GAP);

  transmit.perform();
}

bool Tcl112Climate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header
  if (!data.expect_item(TCL112_HEADER_MARK, TCL112_HEADER_SPACE)) {
    ESP_LOGVV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[TCL112_STATE_LENGTH];
  if (!data.expect_bytes<TCL112_BIT_TIMINGS>(remote_state, TCL112_STATE_LENGTH)) {
    ESP_LOGVV(TAG, "Data fail");
    return false;
  }
  // Validate footer
  if (!data.expect_mark(TCL112_BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  const uint8_t checksum = sum_bytes(remote_state, TCL112_STATE_LENGTH - 1);
  if (checksum != remote_state[TCL112_STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  ESP_LOGV_HEX(TAG, remote_state, TCL112_STATE_LENGTH, TCL112_STATE_LENGTH, "Received: %s");

  // two first bytes are constant
  if (remote_state[0] != 0x23 || remote_state[1] != 0xCB)
    return false;

  if ((remote_state[5] & TCL112_POWER_MASK) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    if (const auto entry = find_ir_code(MODES, remote_state[6] & 0x0F, true))
      this->mode = static_cast<climate::ClimateMode>(entry->value);
  }
  auto temp = TCL112_TEMP_MAX - remote_state[7];
  if (remote_state[12] & TCL112_HALF_DEGREE)
    temp += .5f;
  this->target_temperature = temp;
  this->fan_mode = from_ir_code<climate::ClimateFanMode>(FANS, remote_state[8] & 0x7);
  if ((remote_state[8] & TCL112_VSWING_MASK) == TCL112_VSWING_MASK) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::tcl112
