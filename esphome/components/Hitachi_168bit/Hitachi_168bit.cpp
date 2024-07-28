#include "hitachi_168bit.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hitachi_168bit {

static const char *const TAG = "hitachi_168bit.climate";

const uint16_t hitachi_168bit_HEADER_MARK = 9000;
const uint16_t hitachi_168bit_HEADER_SPACE = 4494;
const uint16_t hitachi_168bit_BIT_MARK = 610;
const uint16_t hitachi_168bit_ONE_SPACE = 1680;
const uint16_t hitachi_168bit_ZERO_SPACE = 565;
const uint32_t hitachi_168bit_GAP = 8007;

const uint32_t hitachi_168bit_CARRIER_FREQUENCY = 38000;

const uint8_t hitachi_168bit_STATE_LENGTH = 21;

const uint8_t hitachi_168bit_HEAT = 0;
const uint8_t hitachi_168bit_DRY = 3;
const uint8_t hitachi_168bit_COOL = 2;
const uint8_t hitachi_168bit_FAN = 4;
const uint8_t hitachi_168bit_AUTO = 1;

const uint8_t hitachi_168bit_FAN_AUTO = 0;
const uint8_t hitachi_168bit_FAN_HIGH = 1;
const uint8_t hitachi_168bit_FAN_MED = 2;
const uint8_t hitachi_168bit_FAN_LOW = 3;

const uint8_t hitachi_168bit_SWING_MASK = 128; //TBD ON HITACHI

const uint8_t hitachi_168bit_POWER = 0x04;

void hitachi_168bitClimate::transmit_state() {
  this->last_transmit_time_ = millis();  // setting the time of the last transmission.
  uint8_t remote_state[hitachi_168bit_STATE_LENGTH] = {0};
  remote_state[0] = 0x95;
  remote_state[1] = 0x9A;
  remote_state[6] = 0x01;
  // MODEL DG11J191
  remote_state[18] = 0x1C; //or 14?

  auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;
  if (powered_on != this->powered_on_assumed) {
    // Set power toggle command
    remote_state[2] = 4;
    remote_state[15] = 1;
    this->powered_on_assumed = powered_on;
  }
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      // set fan auto
      // set temp auto temp
      // set sleep false
      remote_state[3] = hitachi_168bit_AUTO;
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = hitachi_168bit_HEAT;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = hitachi_168bit_COOL;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = hitachi_168bit_DRY;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = hitachi_168bit_FAN;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  // Temperature
  auto temp = (uint8_t) roundf(clamp(this->target_temperature, this->temperature_min_(), this->temperature_max_()));
  remote_state[3] |= (uint8_t) (temp - this->temperature_min_()) << 4;

  // Fan speed
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= hitachi_168bit_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= hitachi_168bit_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= hitachi_168bit_FAN_LOW;
      break;
    default:
      break;
  }

  // Swing
  ESP_LOGV(TAG, "send swing %s", this->send_swing_cmd_ ? "true" : "false");
  if (this->send_swing_cmd_) {
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_OFF) {
      remote_state[2] |= 128;
      remote_state[8] |= 64;
    }
  }

  // Checksum
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

  // Send code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(hitachi_168bit_HEADER_MARK);
  data->space(hitachi_168bit_HEADER_SPACE);
  // Data
  auto bytes_sent = 0;
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(hitachi_168bit_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? hitachi_168bit_ONE_SPACE : hitachi_168bit_ZERO_SPACE);
    }
    bytes_sent++;
    if (bytes_sent == 6 || bytes_sent == 14) {
      // Divider
      data->mark(hitachi_168bit_BIT_MARK);
      data->space(hitachi_168bit_GAP);
    }
  }
  // Footer
  data->mark(hitachi_168bit_BIT_MARK);

  transmit.perform();
}

bool hitachi_168bitClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Check if the esp isn't currently transmitting.
  if (millis() - this->last_transmit_time_ < 500) {
    ESP_LOGV(TAG, "Blocked receive because of current trasmittion");
    return false;
  }

  // Validate header
  if (!data.expect_item(hitachi_168bit_HEADER_MARK, hitachi_168bit_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[hitachi_168bit_STATE_LENGTH] = {0};
  // Read all bytes.
  for (int i = 0; i < hitachi_168bit_STATE_LENGTH; i++) {
    // Read bit
    if (i == 6 || i == 14) {
      if (!data.expect_item(hitachi_168bit_BIT_MARK, hitachi_168bit_GAP))
        return false;
    }
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(hitachi_168bit_BIT_MARK, hitachi_168bit_ONE_SPACE)) {
        remote_state[i] |= 1 << j;

      } else if (!data.expect_item(hitachi_168bit_BIT_MARK, hitachi_168bit_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }

    ESP_LOGVV(TAG, "Byte %d %02X", i, remote_state[i]);
  }
  // Validate footer
  if (!data.expect_mark(hitachi_168bit_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  uint8_t checksum13 = 0;
  uint8_t checksum20 = 0;
  // Calculate  checksum and compare with signal value.
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

  // verify header remote code
  if (remote_state[0] != 0x83 || remote_state[1] != 0x06)
    return false;

  // powr on/off button
  ESP_LOGV(TAG, "Power: %02X", (remote_state[2] & hitachi_168bit_POWER));

  if ((remote_state[2] & hitachi_168bit_POWER) == hitachi_168bit_POWER) {
    auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;

    if (powered_on) {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->powered_on_assumed = false;
    } else {
      this->powered_on_assumed = true;
    }
  }

  // Set received mode
  if (powered_on_assumed) {
    auto mode = remote_state[3] & 0x7;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    switch (mode) {
      case hitachi_168bit_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case hitachi_168bit_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case hitachi_168bit_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case hitachi_168bit_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case hitachi_168bit_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  // Set received temp
  int temp = remote_state[3] & 0xF0;
  ESP_LOGVV(TAG, "Temperature Raw: %02X", temp);
  temp = (uint8_t) temp >> 4;
  temp += static_cast<int>(this->temperature_min_());
  ESP_LOGVV(TAG, "Temperature Climate: %u", temp);
  this->target_temperature = temp;

  // Set received fan speed
  auto fan = remote_state[2] & 0x03;
  ESP_LOGVV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case hitachi_168bit_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case hitachi_168bit_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case hitachi_168bit_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case hitachi_168bit_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Set received swing status
  if ((remote_state[2] & hitachi_168bit_SWING_MASK) == hitachi_168bit_SWING_MASK && remote_state[8] == 0x40) {
    ESP_LOGVV(TAG, "Swing toggle pressed ");
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
