#include "fujitsu_general.h"

namespace esphome::fujitsu_general {

static const char *const TAG = "fujitsu_general.climate";

// Common header
const uint8_t FUJITSU_GENERAL_COMMON_LENGTH = 6;
const uint8_t FUJITSU_GENERAL_COMMON_BYTE0 = 0x14;
const uint8_t FUJITSU_GENERAL_COMMON_BYTE1 = 0x63;
const uint8_t FUJITSU_GENERAL_COMMON_BYTE2 = 0x00;
const uint8_t FUJITSU_GENERAL_COMMON_BYTE3 = 0x10;
const uint8_t FUJITSU_GENERAL_COMMON_BYTE4 = 0x10;
const uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_BYTE = 5;

// State message - temp & fan etc.
const uint8_t FUJITSU_GENERAL_STATE_MESSAGE_LENGTH = 16;
const uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_STATE = 0xFE;

// Util messages - off & eco etc.
const uint8_t FUJITSU_GENERAL_UTIL_MESSAGE_LENGTH = 7;
const uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_OFF = 0x02;
const uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY = 0x09;
const uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE = 0x6C;

// State header
const uint8_t FUJITSU_GENERAL_STATE_HEADER_BYTE0 = 0x09;
const uint8_t FUJITSU_GENERAL_STATE_HEADER_BYTE1 = 0x30;

// State footer
const uint8_t FUJITSU_GENERAL_STATE_FOOTER_BYTE0 = 0x20;

// Power on
const uint8_t FUJITSU_GENERAL_POWER_OFF = 0x00;
const uint8_t FUJITSU_GENERAL_POWER_ON = 0x01;

// Mode
// The mode occupies only the low three bits of this nibble, see the frame documentation in the
// header. The fourth bit belongs to the clean feature, which doubles as 10 degree heat on the
// ARRAH2E and ARREW4E remotes.
const uint8_t FUJITSU_GENERAL_MODE_MASK = 0b0111;
const uint8_t FUJITSU_GENERAL_CLEAN_BIT = 0b1000;
const uint8_t FUJITSU_GENERAL_MODE_AUTO = 0x00;
const uint8_t FUJITSU_GENERAL_MODE_COOL = 0x01;
const uint8_t FUJITSU_GENERAL_MODE_DRY = 0x02;
const uint8_t FUJITSU_GENERAL_MODE_FAN = 0x03;
const uint8_t FUJITSU_GENERAL_MODE_HEAT = 0x04;

// Swing
const uint8_t FUJITSU_GENERAL_SWING_MASK = 0b0011;
const uint8_t FUJITSU_GENERAL_SWING_NONE = 0x00;
const uint8_t FUJITSU_GENERAL_SWING_VERTICAL = 0x01;
const uint8_t FUJITSU_GENERAL_SWING_HORIZONTAL = 0x02;
const uint8_t FUJITSU_GENERAL_SWING_BOTH = 0x03;

// Fan
// Like the mode, the fan speed occupies only the low three bits of its nibble. The fourth bit is
// not assigned by the protocol.
const uint8_t FUJITSU_GENERAL_FAN_MASK = 0b0111;
const uint8_t FUJITSU_GENERAL_FAN_AUTO = 0x00;
const uint8_t FUJITSU_GENERAL_FAN_HIGH = 0x01;
const uint8_t FUJITSU_GENERAL_FAN_MEDIUM = 0x02;
const uint8_t FUJITSU_GENERAL_FAN_LOW = 0x03;
const uint8_t FUJITSU_GENERAL_FAN_SILENT = 0x04;

// TODO Outdoor Unit Low Noise
// const uint8_t FUJITSU_GENERAL_OUTDOOR_UNIT_LOW_NOISE_BYTE14 = 0xA0;
// const uint8_t FUJITSU_GENERAL_STATE_BYTE14 = 0x20;

const uint16_t FUJITSU_GENERAL_HEADER_MARK = 3300;
const uint16_t FUJITSU_GENERAL_HEADER_SPACE = 1600;

const uint16_t FUJITSU_GENERAL_BIT_MARK = 420;
const uint16_t FUJITSU_GENERAL_ONE_SPACE = 1200;
const uint16_t FUJITSU_GENERAL_ZERO_SPACE = 420;

const uint16_t FUJITSU_GENERAL_TRL_MARK = 420;
const uint16_t FUJITSU_GENERAL_TRL_SPACE = 8000;

const uint32_t FUJITSU_GENERAL_CARRIER_FREQUENCY = 38000;

void FujitsuGeneralClimate::transmit_state() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
    return;
  }

  ESP_LOGV(TAG, "Transmit state");

  uint8_t remote_state[FUJITSU_GENERAL_STATE_MESSAGE_LENGTH] = {0};

  // Common message header
  remote_state[0] = FUJITSU_GENERAL_COMMON_BYTE0;
  remote_state[1] = FUJITSU_GENERAL_COMMON_BYTE1;
  remote_state[2] = FUJITSU_GENERAL_COMMON_BYTE2;
  remote_state[3] = FUJITSU_GENERAL_COMMON_BYTE3;
  remote_state[4] = FUJITSU_GENERAL_COMMON_BYTE4;
  remote_state[5] = FUJITSU_GENERAL_MESSAGE_TYPE_STATE;
  remote_state[6] = FUJITSU_GENERAL_STATE_HEADER_BYTE0;
  remote_state[7] = FUJITSU_GENERAL_STATE_HEADER_BYTE1;

  // unknown, does not appear to change with any remote settings
  remote_state[14] = FUJITSU_GENERAL_STATE_FOOTER_BYTE0;

  // Set temperature
  uint8_t temperature_clamped =
      (uint8_t) roundf(clamp<float>(this->target_temperature, FUJITSU_GENERAL_TEMP_MIN, FUJITSU_GENERAL_TEMP_MAX));
  uint8_t temperature_offset = temperature_clamped - FUJITSU_GENERAL_TEMP_MIN;
  set_nibble(remote_state, FUJITSU_GENERAL_TEMPERATURE_NIBBLE, temperature_offset);

  // Set power on
  if (!this->power_) {
    set_nibble(remote_state, FUJITSU_GENERAL_POWER_ON_NIBBLE, FUJITSU_GENERAL_POWER_ON);
  }

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      set_nibble(remote_state, FUJITSU_GENERAL_MODE_NIBBLE, FUJITSU_GENERAL_MODE_COOL);
      break;
    case climate::CLIMATE_MODE_HEAT:
      set_nibble(remote_state, FUJITSU_GENERAL_MODE_NIBBLE, FUJITSU_GENERAL_MODE_HEAT);
      break;
    case climate::CLIMATE_MODE_DRY:
      set_nibble(remote_state, FUJITSU_GENERAL_MODE_NIBBLE, FUJITSU_GENERAL_MODE_DRY);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      set_nibble(remote_state, FUJITSU_GENERAL_MODE_NIBBLE, FUJITSU_GENERAL_MODE_FAN);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      set_nibble(remote_state, FUJITSU_GENERAL_MODE_NIBBLE, FUJITSU_GENERAL_MODE_AUTO);
      break;
  }

  // Set fan
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
    case climate::CLIMATE_FAN_HIGH:
      set_nibble(remote_state, FUJITSU_GENERAL_FAN_NIBBLE, FUJITSU_GENERAL_FAN_HIGH);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      set_nibble(remote_state, FUJITSU_GENERAL_FAN_NIBBLE, FUJITSU_GENERAL_FAN_MEDIUM);
      break;
    case climate::CLIMATE_FAN_LOW:
      set_nibble(remote_state, FUJITSU_GENERAL_FAN_NIBBLE, FUJITSU_GENERAL_FAN_LOW);
      break;
    case climate::CLIMATE_FAN_QUIET:
      set_nibble(remote_state, FUJITSU_GENERAL_FAN_NIBBLE, FUJITSU_GENERAL_FAN_SILENT);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      set_nibble(remote_state, FUJITSU_GENERAL_FAN_NIBBLE, FUJITSU_GENERAL_FAN_AUTO);
      break;
  }

  // Set swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      set_nibble(remote_state, FUJITSU_GENERAL_SWING_NIBBLE, FUJITSU_GENERAL_SWING_VERTICAL);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      set_nibble(remote_state, FUJITSU_GENERAL_SWING_NIBBLE, FUJITSU_GENERAL_SWING_HORIZONTAL);
      break;
    case climate::CLIMATE_SWING_BOTH:
      set_nibble(remote_state, FUJITSU_GENERAL_SWING_NIBBLE, FUJITSU_GENERAL_SWING_BOTH);
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      set_nibble(remote_state, FUJITSU_GENERAL_SWING_NIBBLE, FUJITSU_GENERAL_SWING_NONE);
      break;
  }

  // TODO: missing support for outdoor unit low noise
  // remote_state[14] = (byte) remote_state[14] | FUJITSU_GENERAL_OUTDOOR_UNIT_LOW_NOISE_BYTE14;

  remote_state[FUJITSU_GENERAL_STATE_MESSAGE_LENGTH - 1] = this->checksum_state_(remote_state);

  this->transmit_(remote_state, FUJITSU_GENERAL_STATE_MESSAGE_LENGTH);

  this->power_ = true;
}

void FujitsuGeneralClimate::transmit_off_() {
  ESP_LOGV(TAG, "Transmit off");

  uint8_t remote_state[FUJITSU_GENERAL_UTIL_MESSAGE_LENGTH] = {0};

  remote_state[0] = FUJITSU_GENERAL_COMMON_BYTE0;
  remote_state[1] = FUJITSU_GENERAL_COMMON_BYTE1;
  remote_state[2] = FUJITSU_GENERAL_COMMON_BYTE2;
  remote_state[3] = FUJITSU_GENERAL_COMMON_BYTE3;
  remote_state[4] = FUJITSU_GENERAL_COMMON_BYTE4;
  remote_state[5] = FUJITSU_GENERAL_MESSAGE_TYPE_OFF;
  remote_state[6] = this->checksum_util_(remote_state);

  this->transmit_(remote_state, FUJITSU_GENERAL_UTIL_MESSAGE_LENGTH);

  this->power_ = false;
}

void FujitsuGeneralClimate::transmit_(uint8_t const *message, uint8_t length) {
  ESP_LOGV(TAG, "Transmit message length %d", length);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(FUJITSU_GENERAL_CARRIER_FREQUENCY);

  // Header
  data->mark(FUJITSU_GENERAL_HEADER_MARK);
  data->space(FUJITSU_GENERAL_HEADER_SPACE);

  // Data
  for (uint8_t i = 0; i < length; ++i) {
    const uint8_t byte = message[i];
    for (uint8_t mask = 0b00000001; mask > 0; mask <<= 1) {  // write from right to left
      data->mark(FUJITSU_GENERAL_BIT_MARK);
      bool bit = byte & mask;
      data->space(bit ? FUJITSU_GENERAL_ONE_SPACE : FUJITSU_GENERAL_ZERO_SPACE);
    }
  }

  // Footer
  data->mark(FUJITSU_GENERAL_TRL_MARK);
  data->space(FUJITSU_GENERAL_TRL_SPACE);

  transmit.perform();
}

uint8_t FujitsuGeneralClimate::checksum_state_(uint8_t const *message) {
  uint8_t checksum = 0;
  for (uint8_t i = 7; i < FUJITSU_GENERAL_STATE_MESSAGE_LENGTH - 1; ++i) {
    checksum += message[i];
  }
  return 256 - checksum;
}

uint8_t FujitsuGeneralClimate::checksum_util_(uint8_t const *message) { return 255 - message[5]; }

climate::ClimateMode decode_mode(uint8_t mode_field, climate::ClimateMode current_mode) {
  switch (mode_field & FUJITSU_GENERAL_MODE_MASK) {
    case FUJITSU_GENERAL_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case FUJITSU_GENERAL_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case FUJITSU_GENERAL_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case FUJITSU_GENERAL_MODE_FAN:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case FUJITSU_GENERAL_MODE_AUTO:
      return climate::CLIMATE_MODE_HEAT_COOL;
    default:
      // The protocol does not assign these values, so there is nothing to report. Keeping the
      // current mode claims less than inventing one. A state frame does describe a running unit
      // though, so off is not an answer this can give: reporting it would publish a unit that is
      // running as off, and make the next transmission a power off command.
      ESP_LOGW(TAG, "Received unassigned mode %X, keeping the current mode", mode_field & FUJITSU_GENERAL_MODE_MASK);
      return current_mode == climate::CLIMATE_MODE_OFF ? climate::CLIMATE_MODE_HEAT_COOL : current_mode;
  }
}

optional<climate::ClimateFanMode> decode_fan_mode(uint8_t fan_field, optional<climate::ClimateFanMode> current_mode) {
  switch (fan_field & FUJITSU_GENERAL_FAN_MASK) {
    case FUJITSU_GENERAL_FAN_HIGH:
      return climate::CLIMATE_FAN_HIGH;
    case FUJITSU_GENERAL_FAN_MEDIUM:
      return climate::CLIMATE_FAN_MEDIUM;
    case FUJITSU_GENERAL_FAN_LOW:
      return climate::CLIMATE_FAN_LOW;
    case FUJITSU_GENERAL_FAN_SILENT:
      return climate::CLIMATE_FAN_QUIET;
    case FUJITSU_GENERAL_FAN_AUTO:
      return climate::CLIMATE_FAN_AUTO;
    default:
      ESP_LOGW(TAG, "Received unassigned fan speed %X, keeping the current fan mode",
               fan_field & FUJITSU_GENERAL_FAN_MASK);
      return current_mode;
  }
}

climate::ClimateSwingMode decode_swing_mode(uint8_t swing_field) {
  switch (swing_field & FUJITSU_GENERAL_SWING_MASK) {
    case FUJITSU_GENERAL_SWING_VERTICAL:
      return climate::CLIMATE_SWING_VERTICAL;
    case FUJITSU_GENERAL_SWING_HORIZONTAL:
      return climate::CLIMATE_SWING_HORIZONTAL;
    case FUJITSU_GENERAL_SWING_BOTH:
      return climate::CLIMATE_SWING_BOTH;
    case FUJITSU_GENERAL_SWING_NONE:
    default:
      // The mask leaves two bits and the protocol assigns all four of their values, so unlike the
      // mode and the fan speed there is no unassigned value to report here.
      return climate::CLIMATE_SWING_OFF;
  }
}

bool FujitsuGeneralClimate::on_receive(remote_base::RemoteReceiveData data) {
  ESP_LOGV(TAG, "Received IR message");

  // Validate header
  if (!data.expect_item(FUJITSU_GENERAL_HEADER_MARK, FUJITSU_GENERAL_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t recv_message[FUJITSU_GENERAL_STATE_MESSAGE_LENGTH] = {0};

  // Read header
  for (uint8_t byte = 0; byte < FUJITSU_GENERAL_COMMON_LENGTH; ++byte) {
    // Read bit
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (data.expect_item(FUJITSU_GENERAL_BIT_MARK, FUJITSU_GENERAL_ONE_SPACE)) {
        recv_message[byte] |= 1 << bit;  // read from right to left
      } else if (!data.expect_item(FUJITSU_GENERAL_BIT_MARK, FUJITSU_GENERAL_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", byte, bit);
        return false;
      }
    }
  }

  const uint8_t recv_message_type = recv_message[FUJITSU_GENERAL_MESSAGE_TYPE_BYTE];
  uint8_t recv_message_length;

  switch (recv_message_type) {
    case FUJITSU_GENERAL_MESSAGE_TYPE_STATE:
      ESP_LOGV(TAG, "Received state message");
      recv_message_length = FUJITSU_GENERAL_STATE_MESSAGE_LENGTH;
      break;
    case FUJITSU_GENERAL_MESSAGE_TYPE_OFF:
    case FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY:
    case FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE:
      ESP_LOGV(TAG, "Received util message");
      recv_message_length = FUJITSU_GENERAL_UTIL_MESSAGE_LENGTH;
      break;
    default:
      ESP_LOGV(TAG, "Unknown message type %X", recv_message_type);
      return false;
  }

  // Read message body
  for (uint8_t byte = FUJITSU_GENERAL_COMMON_LENGTH; byte < recv_message_length; ++byte) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (data.expect_item(FUJITSU_GENERAL_BIT_MARK, FUJITSU_GENERAL_ONE_SPACE)) {
        recv_message[byte] |= 1 << bit;  // read from right to left
      } else if (!data.expect_item(FUJITSU_GENERAL_BIT_MARK, FUJITSU_GENERAL_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", byte, bit);
        return false;
      }
    }
  }

  for (uint8_t byte = 0; byte < recv_message_length; ++byte) {
    ESP_LOGVV(TAG, "%02X", recv_message[byte]);
  }

  const uint8_t recv_checksum = recv_message[recv_message_length - 1];
  uint8_t calculated_checksum;
  if (recv_message_type == FUJITSU_GENERAL_MESSAGE_TYPE_STATE) {
    calculated_checksum = this->checksum_state_(recv_message);
  } else {
    calculated_checksum = this->checksum_util_(recv_message);
  }

  if (recv_checksum != calculated_checksum) {
    ESP_LOGV(TAG, "Checksum fail - expected %X - got %X", calculated_checksum, recv_checksum);
    return false;
  }

  if (recv_message_type == FUJITSU_GENERAL_MESSAGE_TYPE_STATE) {
    const uint8_t recv_tempertature = get_nibble(recv_message, FUJITSU_GENERAL_TEMPERATURE_NIBBLE);
    const uint8_t offset_temperature = recv_tempertature + FUJITSU_GENERAL_TEMP_MIN;
    this->target_temperature = offset_temperature;
    ESP_LOGV(TAG, "Received temperature %d", offset_temperature);

    const uint8_t recv_mode = get_nibble(recv_message, FUJITSU_GENERAL_MODE_NIBBLE);
    ESP_LOGV(TAG, "Received mode %X", recv_mode);
    if ((recv_mode & FUJITSU_GENERAL_CLEAN_BIT) != 0) {
      ESP_LOGW(TAG, "Received a frame with the clean / 10 degree heat bit set, which is not supported");
    }
    this->mode = decode_mode(recv_mode, this->mode);

    const uint8_t recv_fan_mode = get_nibble(recv_message, FUJITSU_GENERAL_FAN_NIBBLE);
    ESP_LOGV(TAG, "Received fan mode %X", recv_fan_mode);
    this->fan_mode = decode_fan_mode(recv_fan_mode, this->fan_mode);

    const uint8_t recv_swing_mode = get_nibble(recv_message, FUJITSU_GENERAL_SWING_NIBBLE);
    ESP_LOGV(TAG, "Received swing mode %X", recv_swing_mode);
    this->swing_mode = decode_swing_mode(recv_swing_mode);

    this->power_ = true;
  }

  else if (recv_message_type == FUJITSU_GENERAL_MESSAGE_TYPE_OFF) {
    ESP_LOGV(TAG, "Received off message");
    this->mode = climate::CLIMATE_MODE_OFF;
    this->power_ = false;
  }

  else {
    ESP_LOGV(TAG, "Received unsupprted message type %X", recv_message_type);
    return false;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::fujitsu_general
