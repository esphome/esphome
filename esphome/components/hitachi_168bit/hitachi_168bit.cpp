#include "hitachi_168bit.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hitachi_168bit {

ESPHOME_LOG_TAG(TAG, "hitachi_168bit.climate");

static constexpr uint16_t HITACHI168BIT_HEADER_MARK = 9000;
static constexpr uint16_t HITACHI168BIT_HEADER_SPACE = 4494;
static constexpr uint16_t HITACHI168BIT_BIT_MARK = 600;
static constexpr remote_base::BitTimings HITACHI168BIT_BIT_TIMINGS{
    .bit_mark = HITACHI168BIT_BIT_MARK, .one_space = 1745, .zero_space = 580};
static constexpr uint32_t HITACHI168BIT_GAP = 8007;

static constexpr uint32_t HITACHI168BIT_CARRIER_FREQUENCY = 38000;

static constexpr uint8_t HITACHI168BIT_STATE_LENGTH = 21;
static constexpr uint8_t HITACHI168BIT_FIRST_DIVIDER = 6;
static constexpr uint8_t HITACHI168BIT_SECOND_DIVIDER = 14;

// Remote identifier, bytes 0 and 1
static constexpr uint8_t HITACHI168BIT_ID_0 = 0xA9;
static constexpr uint8_t HITACHI168BIT_ID_1 = 0x59;

// Mode, low bits of byte 3
static constexpr uint8_t HITACHI168BIT_HEAT = 0;
static constexpr uint8_t HITACHI168BIT_AUTO = 1;
static constexpr uint8_t HITACHI168BIT_COOL = 2;
static constexpr uint8_t HITACHI168BIT_DRY = 3;
static constexpr uint8_t HITACHI168BIT_FAN = 4;
static constexpr uint8_t HITACHI168BIT_MODE_MASK = 0x07;

// Fan speed, low bits of byte 2
static constexpr uint8_t HITACHI168BIT_FAN_AUTO = 0;
static constexpr uint8_t HITACHI168BIT_FAN_HIGH = 1;
static constexpr uint8_t HITACHI168BIT_FAN_MED = 2;
static constexpr uint8_t HITACHI168BIT_FAN_LOW = 3;
static constexpr uint8_t HITACHI168BIT_FAN_MASK = 0x03;

static constexpr uint8_t HITACHI168BIT_POWER = 0x04;        // byte 2
static constexpr uint8_t HITACHI168BIT_SWING_MASK = 0x80;   // byte 2
static constexpr uint8_t HITACHI168BIT_SWING_BYTE8 = 0x40;  // byte 8
static constexpr uint8_t HITACHI168BIT_TEMP_MASK = 0xF0;    // byte 3, steps above the minimum
static constexpr uint8_t HITACHI168BIT_BYTE18_ON = 0x38;    // byte 18 while running
static constexpr uint8_t HITACHI168BIT_BYTE18_OFF = 0x28;   // byte 18 when turning off

/// Ignore received frames for this long after transmitting, so the unit does not hear its own echo.
static constexpr uint32_t HITACHI168BIT_RECEIVE_BLOCK_MS = 500;

Hitachi168bitClimate::Hitachi168bitClimate()
    : climate_ir::ClimateIR(
          HITACHI168BIT_HCRA31NEWH_TEMP_MIN, HITACHI168BIT_HCRA31NEWH_TEMP_MAX, 1.0f, true, true,
          {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
          {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

void Hitachi168bitClimate::transmit_state() {
  this->last_transmit_time_ = millis();
  uint8_t remote_state[HITACHI168BIT_STATE_LENGTH] = {0};
  remote_state[0] = HITACHI168BIT_ID_0;
  remote_state[1] = HITACHI168BIT_ID_1;
  remote_state[6] = 0x80;
  remote_state[10] = 0x20;
  remote_state[18] = HITACHI168BIT_BYTE18_ON;
  remote_state[19] = 0x08;

  // The unit takes the full state in every frame, so a power change only needs tracking here.
  this->powered_on_assumed = this->mode != climate::CLIMATE_MODE_OFF;

  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[3] = HITACHI168BIT_AUTO;
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = HITACHI168BIT_HEAT;
      remote_state[15] = 0x06;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = HITACHI168BIT_COOL;
      remote_state[15] = 0x02;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = HITACHI168BIT_DRY;
      remote_state[15] = 0x06;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = HITACHI168BIT_FAN;
      remote_state[15] = 0x06;
      break;
    case climate::CLIMATE_MODE_OFF:
      remote_state[2] = HITACHI168BIT_POWER;
      remote_state[3] = HITACHI168BIT_COOL;
      remote_state[15] = 0x01;
      remote_state[18] = HITACHI168BIT_BYTE18_OFF;
      break;
    default:
      break;
  }

  // Temperature
  const auto temp =
      static_cast<uint8_t>(roundf(clamp(this->target_temperature, this->temperature_min_(), this->temperature_max_())));
  remote_state[3] |= static_cast<uint8_t>(temp - static_cast<uint8_t>(this->temperature_min_())) << 4;

  // Fan speed
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= HITACHI168BIT_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= HITACHI168BIT_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= HITACHI168BIT_FAN_LOW;
      break;
    default:
      break;
  }

  // Swing
  ESP_LOGV(TAG, "Send swing %s", this->send_swing_cmd_ ? LOG_STR_LITERAL("true") : LOG_STR_LITERAL("false"));
  if (this->send_swing_cmd_) {
    remote_state[2] |= HITACHI168BIT_SWING_MASK;
    remote_state[8] |= HITACHI168BIT_SWING_BYTE8;
  }

  // Checksums
  for (uint8_t i = 2; i < 13; i++)
    remote_state[13] ^= remote_state[i];
  for (uint8_t i = 14; i < 20; i++)
    remote_state[20] ^= remote_state[i];

  ESP_LOGV_HEX(TAG, remote_state, HITACHI168BIT_STATE_LENGTH, HITACHI168BIT_STATE_LENGTH, "Sending: %s");

  // Send code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(HITACHI168BIT_CARRIER_FREQUENCY);

  // Header
  data->mark(HITACHI168BIT_HEADER_MARK);
  data->space(HITACHI168BIT_HEADER_SPACE);
  // Data
  for (uint8_t i = 0; i < HITACHI168BIT_STATE_LENGTH; i++) {
    if (i == HITACHI168BIT_FIRST_DIVIDER || i == HITACHI168BIT_SECOND_DIVIDER) {
      // Divider
      data->mark(HITACHI168BIT_BIT_MARK);
      data->space(HITACHI168BIT_GAP);
    }
    data->item_bytes<HITACHI168BIT_BIT_TIMINGS>(remote_state + i, 1);
  }
  // Footer
  data->mark(HITACHI168BIT_BIT_MARK);

  transmit.perform();
}

bool Hitachi168bitClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Check that the ESP isn't currently transmitting.
  if (millis() - this->last_transmit_time_ < HITACHI168BIT_RECEIVE_BLOCK_MS) {
    ESP_LOGV(TAG, "Blocked receive because of current transmission");
    return false;
  }

  // Validate header
  if (!data.expect_item(HITACHI168BIT_HEADER_MARK, HITACHI168BIT_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[HITACHI168BIT_STATE_LENGTH] = {0};
  // Read all bytes
  for (uint8_t i = 0; i < HITACHI168BIT_STATE_LENGTH; i++) {
    if (i == HITACHI168BIT_FIRST_DIVIDER || i == HITACHI168BIT_SECOND_DIVIDER) {
      if (!data.expect_item(HITACHI168BIT_BIT_MARK, HITACHI168BIT_GAP))
        return false;
    }
    if (!data.expect_bytes<HITACHI168BIT_BIT_TIMINGS>(remote_state + i, 1)) {
      ESP_LOGV(TAG, "Byte %u fail", i);
      return false;
    }
  }
  // Validate footer
  if (!data.expect_mark(HITACHI168BIT_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // Validate remote identifier
  if (remote_state[0] != HITACHI168BIT_ID_0 || remote_state[1] != HITACHI168BIT_ID_1)
    return false;

  // Validate checksums
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

  ESP_LOGV_HEX(TAG, remote_state, HITACHI168BIT_STATE_LENGTH, HITACHI168BIT_STATE_LENGTH, "Received: %s");

  // Power on/off button
  ESP_LOGV(TAG, "Power: %02X", remote_state[2] & HITACHI168BIT_POWER);
  if ((remote_state[2] & HITACHI168BIT_POWER) == HITACHI168BIT_POWER) {
    if (this->mode != climate::CLIMATE_MODE_OFF) {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->powered_on_assumed = false;
    } else {
      this->powered_on_assumed = true;
    }
  }

  // Set received mode
  if (this->powered_on_assumed) {
    const uint8_t mode = remote_state[3] & HITACHI168BIT_MODE_MASK;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    switch (mode) {
      case HITACHI168BIT_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case HITACHI168BIT_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case HITACHI168BIT_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case HITACHI168BIT_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case HITACHI168BIT_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        break;
    }
  }

  // Set received temperature
  const uint8_t temp_step = (remote_state[3] & HITACHI168BIT_TEMP_MASK) >> 4;
  this->target_temperature = this->temperature_min_() + temp_step;
  ESP_LOGVV(TAG, "Temperature: %.0f", this->target_temperature);

  // Set received fan speed
  const uint8_t fan = remote_state[2] & HITACHI168BIT_FAN_MASK;
  ESP_LOGVV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case HITACHI168BIT_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case HITACHI168BIT_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case HITACHI168BIT_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case HITACHI168BIT_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Set received swing status
  if ((remote_state[2] & HITACHI168BIT_SWING_MASK) == HITACHI168BIT_SWING_MASK &&
      remote_state[8] == HITACHI168BIT_SWING_BYTE8) {
    ESP_LOGVV(TAG, "Swing toggle pressed");
    this->swing_mode =
        this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::hitachi_168bit
