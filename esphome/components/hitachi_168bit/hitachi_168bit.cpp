#include "hitachi_168bit.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hitachi_168bit {

static const char *const TAG = "hitachi_168bit.climate";

// Timing & protocol constants
constexpr uint16_t HEADER_MARK = 9000;
constexpr uint16_t HEADER_SPACE = 4494;
constexpr uint16_t BIT_MARK = 610;
constexpr uint16_t ONE_SPACE = 1680;
constexpr uint16_t ZERO_SPACE = 565;
constexpr uint32_t GAP = 8007;
constexpr uint32_t CARRIER_FREQUENCY = 38000;

// Frame structure
constexpr uint8_t STATE_LENGTH = 21;

// Mode nibble values
constexpr uint8_t MODE_HEAT = 0;
constexpr uint8_t MODE_DRY = 3;
constexpr uint8_t MODE_COOL = 2;
constexpr uint8_t MODE_FAN = 4;
constexpr uint8_t MODE_AUTO = 1;

// Fan values (2 LSBits of byte[2])
constexpr uint8_t FAN_AUTO = 0;
constexpr uint8_t FAN_HIGH = 1;
constexpr uint8_t FAN_MED = 2;
constexpr uint8_t FAN_LOW = 3;

// Swing & power flags
constexpr uint8_t SWING_MASK = 0x80;  // Not Tested in HITACHI
constexpr uint8_t POWER_FLAG = 0x04;

void Hitachi168bitClimate::transmit_state() {
  this->last_transmit_time_ = millis();  // timestamp last transmission

  uint8_t remote_state[STATE_LENGTH] = {0};
  remote_state[0] = 0x95;
  remote_state[1] = 0x9A;
  remote_state[6] = 0x01;

  remote_state[18] = 0x1C;  // (or 0x14?) keep as discovered

  const bool powered_on = (this->mode != climate::CLIMATE_MODE_OFF);
  if (powered_on != this->powered_on_assumed) {
    // Set power toggle command
    remote_state[2] = POWER_FLAG;
    remote_state[15] = 1;
    this->powered_on_assumed = powered_on;
  }

  // Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[3] = MODE_AUTO;  // auto mode
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = MODE_HEAT;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = MODE_COOL;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = MODE_DRY;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = MODE_FAN;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  // Temperature nibble (upper 4 bits of byte[3])
  const uint8_t temp =
      static_cast<uint8_t>(roundf(clamp(this->target_temperature, this->temperature_min_(), this->temperature_max_())));
  remote_state[3] |= static_cast<uint8_t>(temp - this->temperature_min_()) << 4;

  // Fan speed (lower 2 bits of byte[2])
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= FAN_LOW;
      break;
    default:
      // auto: leave as 0
      break;
  }

  // Swing (toggle semantics on this protocol)
  ESP_LOGV(TAG, "send swing %s", this->send_swing_cmd_ ? "true" : "false");
  if (this->send_swing_cmd_) {
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_OFF) {
      remote_state[2] |= SWING_MASK;
      remote_state[8] |= 0x40;
    }
  }

  // Checksums
  for (uint8_t i = 2; i < 13; i++)
    remote_state[13] ^= remote_state[i];
  for (uint8_t i = 14; i < 20; i++)
    remote_state[20] ^= remote_state[i];

  ESP_LOGV(TAG,
           "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X "
           "%02X %02X   %02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
           remote_state[12], remote_state[13], remote_state[14], remote_state[15], remote_state[16], remote_state[17],
           remote_state[18], remote_state[19], remote_state[20]);

  // Transmit
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(CARRIER_FREQUENCY);

  // Header
  data->mark(HEADER_MARK);
  data->space(HEADER_SPACE);

  // Data
  uint8_t bytes_sent = 0;
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(BIT_MARK);
      const bool bit = (i & (1 << j)) != 0;
      data->space(bit ? ONE_SPACE : ZERO_SPACE);
    }
    bytes_sent++;
    if (bytes_sent == 6 || bytes_sent == 14) {
      // Divider
      data->mark(BIT_MARK);
      data->space(GAP);
    }
  }

  // Footer
  data->mark(BIT_MARK);

  transmit.perform();
}

bool Hitachi168bitClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Ignore if we recently transmitted (debounce)
  if (millis() - this->last_transmit_time_ < 500) {
    ESP_LOGV(TAG, "Blocked receive because of current transmission");
    return false;
  }

  // Header
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[STATE_LENGTH] = {0};

  // Read all bytes
  for (int i = 0; i < STATE_LENGTH; i++) {
    // Divider positions
    if (i == 6 || i == 14) {
      if (!data.expect_item(BIT_MARK, GAP))
        return false;
    }
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(BIT_MARK, ONE_SPACE)) {
        remote_state[i] |= 1 << j;
      } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }
    ESP_LOGVV(TAG, "Byte %d %02X", i, remote_state[i]);
  }

  // Footer
  if (!data.expect_mark(BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // Checksums
  uint8_t checksum13 = 0;
  uint8_t checksum20 = 0;
  for (uint8_t i = 2; i < 13; i++)
    checksum13 ^= remote_state[i];
  for (uint8_t i = 14; i < 20; i++)
    checksum20 ^= remote_state[i];

  if (checksum13 != remote_state[13] || checksum20 != remote_state[20]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  ESP_LOGV(
      TAG,
      "Received: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X "
      "%02X %02X   %02X",
      remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
      remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
      remote_state[12], remote_state[13], remote_state[14], remote_state[15], remote_state[16], remote_state[17],
      remote_state[18], remote_state[19], remote_state[20]);

  // Verify header remote code (protocol ID)
  if (remote_state[0] != 0x83 || remote_state[1] != 0x06)
    return false;

  // Power toggle
  ESP_LOGV(TAG, "Power: %02X", (remote_state[2] & POWER_FLAG));
  if ((remote_state[2] & POWER_FLAG) == POWER_FLAG) {
    const bool powered_on = (this->mode != climate::CLIMATE_MODE_OFF);
    if (powered_on) {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->powered_on_assumed = false;
    } else {
      this->powered_on_assumed = true;
    }
  }

  // If powered on, parse mode
  if (this->powered_on_assumed) {
    const uint8_t mode = remote_state[3] & 0x07;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    switch (mode) {
      case MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        break;
    }
  }

  // Temperature
  int temp = remote_state[3] & 0xF0;
  ESP_LOGVV(TAG, "Temperature Raw: %02X", temp);
  temp = static_cast<uint8_t>(temp) >> 4;
  temp += static_cast<int>(this->temperature_min_());
  ESP_LOGVV(TAG, "Temperature Climate: %u", temp);
  this->target_temperature = static_cast<float>(temp);

  // Fan
  const uint8_t fan = remote_state[2] & 0x03;
  ESP_LOGVV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Swing toggle detection
  if ((remote_state[2] & SWING_MASK) == SWING_MASK && remote_state[8] == 0x40) {
    ESP_LOGVV(TAG, "Swing toggle pressed");
    if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  }

  this->publish_state();
  return true;
}

}  // namespace hitachi_168bit
}  // namespace esphome
