#include "tcl112.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl112 {

static const char *const TAG = "tcl112.climate";

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
const uint16_t TCL112_ONE_SPACE = 1100;
const uint16_t TCL112_ZERO_SPACE = 350;
const uint32_t TCL112_GAP = TCL112_HEADER_SPACE;

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
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] &= ~TCL112_POWER_MASK;
      break;
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
  uint8_t selected_fan;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      selected_fan = TCL112_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      selected_fan = TCL112_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      selected_fan = TCL112_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      selected_fan = TCL112_FAN_AUTO;
  }
  remote_state[8] |= selected_fan;

  // Set swing
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    remote_state[8] |= TCL112_VSWING_MASK;
  }

  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  for (uint8_t checksum_byte = 0; checksum_byte < TCL112_STATE_LENGTH - 1; checksum_byte++)
    remote_state[TCL112_STATE_LENGTH - 1] += remote_state[checksum_byte];

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X", remote_state[0],
           remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6],
           remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11], remote_state[12],
           remote_state[13]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(TCL112_HEADER_MARK);
  data->space(TCL112_HEADER_SPACE);
  // Data
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(TCL112_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? TCL112_ONE_SPACE : TCL112_ZERO_SPACE);
    }
  }
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

  uint8_t remote_state[TCL112_STATE_LENGTH] = {0};
  // Read all bytes.
  for (int i = 0; i < TCL112_STATE_LENGTH; i++) {
    // Read bit
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(TCL112_BIT_MARK, TCL112_ONE_SPACE)) {
        remote_state[i] |= 1 << j;
      } else if (!data.expect_item(TCL112_BIT_MARK, TCL112_ZERO_SPACE)) {
        ESP_LOGVV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }
  }
  // Validate footer
  if (!data.expect_mark(TCL112_BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  uint8_t checksum = 0;
  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  for (uint8_t checksum_byte = 0; checksum_byte < TCL112_STATE_LENGTH - 1; checksum_byte++)
    checksum += remote_state[checksum_byte];
  if (checksum != remote_state[TCL112_STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
           remote_state[12], remote_state[13]);

  // two first bytes are constant
  if (remote_state[0] != 0x23 || remote_state[1] != 0xCB)
    return false;

  if ((remote_state[5] & TCL112_POWER_MASK) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    auto mode = remote_state[6] & 0x0F;
    switch (mode) {
      case TCL112_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case TCL112_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case TCL112_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case TCL112_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case TCL112_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }
  auto temp = TCL112_TEMP_MAX - remote_state[7];
  if (remote_state[12] & TCL112_HALF_DEGREE)
    temp += .5f;
  this->target_temperature = temp;
  auto fan = remote_state[8] & 0x7;
  switch (fan) {
    case TCL112_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case TCL112_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case TCL112_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case TCL112_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
  if ((remote_state[8] & TCL112_VSWING_MASK) == TCL112_VSWING_MASK) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace tcl112
}  // namespace esphome
