#include "whirlpool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::whirlpool {

ESPHOME_LOG_TAG(TAG, "whirlpool.climate");

const uint16_t WHIRLPOOL_HEADER_MARK = 9000;
const uint16_t WHIRLPOOL_HEADER_SPACE = 4494;
const uint16_t WHIRLPOOL_BIT_MARK = 572;
const uint16_t WHIRLPOOL_ONE_SPACE = 1659;
const uint16_t WHIRLPOOL_ZERO_SPACE = 553;
const uint32_t WHIRLPOOL_GAP = 7960;

const uint32_t WHIRLPOOL_CARRIER_FREQUENCY = 38000;

const uint8_t WHIRLPOOL_STATE_LENGTH = 21;

const uint8_t WHIRLPOOL_HEAT = 0;
const uint8_t WHIRLPOOL_DRY = 3;
const uint8_t WHIRLPOOL_COOL = 2;
const uint8_t WHIRLPOOL_FAN = 4;
const uint8_t WHIRLPOOL_AUTO = 1;

const uint8_t WHIRLPOOL_FAN_AUTO = 0;
const uint8_t WHIRLPOOL_FAN_HIGH = 1;
const uint8_t WHIRLPOOL_FAN_MED = 2;
const uint8_t WHIRLPOOL_FAN_LOW = 3;

const uint8_t WHIRLPOOL_FAN_MASK = 0x03;

const uint8_t WHIRLPOOL_SWING_MASK = 128;    // byte 2
const uint8_t WHIRLPOOL_SWING_BYTE8 = 0x40;  // byte 8

const uint8_t WHIRLPOOL_POWER = 0x04;

// Hisense ARG model; unused constexpr values cost nothing in DG11J1-only builds
static constexpr uint8_t HISENSE_ARG_DISPLAY_ON = 0x80;     // byte 11
static constexpr uint8_t HISENSE_ARG_AMBIENT_TEMP = 0x18;   // byte 12, 24 C
static constexpr uint8_t HISENSE_ARG_POWER_TOGGLE = 0x01;   // byte 15
static constexpr uint8_t HISENSE_ARG_SETTINGS_ONLY = 0x02;  // byte 15
static constexpr uint8_t HISENSE_ARG_FIXED_TEMP_STEP = 7;   // auto and fan-only send 23 C

WhirlpoolClimate::WhirlpoolClimate()
    : climate_ir::ClimateIR(
          WHIRLPOOL_DG11J1_3A_TEMP_MIN, WHIRLPOOL_DG11J1_3A_TEMP_MAX, 1.0f, true, true,
          {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
          {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}),
      model_(MODEL_DG11J1_3A) {}

void WhirlpoolClimate::transmit_state() {
  this->last_transmit_time_ = millis();  // setting the time of the last transmission.
  uint8_t remote_state[WHIRLPOOL_STATE_LENGTH] = {0};
  remote_state[0] = 0x83;
  remote_state[1] = 0x06;
  remote_state[6] = 0x80;
  remote_state[18] = 0x08;  // the same on every supported model

  auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;
  const bool power_toggle = powered_on != this->powered_on_assumed;
  if (power_toggle) {
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
      remote_state[3] = WHIRLPOOL_AUTO;
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = WHIRLPOOL_HEAT;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = WHIRLPOOL_COOL;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = WHIRLPOOL_DRY;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = WHIRLPOOL_FAN;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  const bool hisense_arg = this->is_hisense_arg_();

  // Temperature
  auto temp = (uint8_t) roundf(clamp(this->target_temperature, this->temperature_min_(), this->temperature_max_()));
  const auto temp_step = (uint8_t) (temp - this->temperature_min_());
  if (hisense_arg && this->fixed_temp_mode_()) {
    remote_state[3] |= HISENSE_ARG_FIXED_TEMP_STEP << 4;
  } else {
    remote_state[3] |= temp_step << 4;
  }

  // Fan speed
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= WHIRLPOOL_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= WHIRLPOOL_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= WHIRLPOOL_FAN_LOW;
      break;
    default:
      break;
  }

  // Swing
  ESP_LOGV(TAG, "send swing %s", this->send_swing_cmd_ ? LOG_STR_LITERAL("true") : LOG_STR_LITERAL("false"));
  if (hisense_arg) {
    // This model sends swing as a state on every frame; the DG11J1 remotes send a toggle
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      remote_state[2] |= WHIRLPOOL_SWING_MASK;
      remote_state[8] = WHIRLPOOL_SWING_BYTE8;
    }
    this->encode_hisense_arg_(remote_state, power_toggle, temp_step);
  } else if (this->send_swing_cmd_) {
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_OFF) {
      remote_state[2] |= WHIRLPOOL_SWING_MASK;
      remote_state[8] |= WHIRLPOOL_SWING_BYTE8;
    }
  }

  // Checksum
  remote_state[13] = xor_bytes(&remote_state[2], 11, remote_state[13]);
  remote_state[20] = xor_bytes(&remote_state[14], 6, remote_state[20]);

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
  data->mark(WHIRLPOOL_HEADER_MARK);
  data->space(WHIRLPOOL_HEADER_SPACE);
  // Data
  for (uint8_t i = 0; i < WHIRLPOOL_STATE_LENGTH; i++) {
    if (i == 6 || i == 14) {
      // Divider
      data->mark(WHIRLPOOL_BIT_MARK);
      data->space(WHIRLPOOL_GAP);
    }
    data->item_bytes_lsb(remote_state + i, 1, WHIRLPOOL_BIT_MARK, WHIRLPOOL_ONE_SPACE, WHIRLPOOL_ZERO_SPACE);
  }
  // Footer
  data->mark(WHIRLPOOL_BIT_MARK);

  transmit.perform();
}

bool WhirlpoolClimate::fixed_temp_mode_() const {
  return this->mode == climate::CLIMATE_MODE_HEAT_COOL || this->mode == climate::CLIMATE_MODE_FAN_ONLY;
}

void WhirlpoolClimate::encode_hisense_arg_(uint8_t *remote_state, bool power_toggle, uint8_t temp_step) const {
#ifdef USE_WHIRLPOOL_HISENSE_ARG
  remote_state[11] = HISENSE_ARG_DISPLAY_ON;
  remote_state[12] = HISENSE_ARG_AMBIENT_TEMP;
  remote_state[15] = power_toggle ? HISENSE_ARG_POWER_TOGGLE : HISENSE_ARG_SETTINGS_ONLY;

  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      // The remote keeps the cool code in the power-off frame
      remote_state[3] = (remote_state[3] & 0xF0) | WHIRLPOOL_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      // The unit runs these fan speeds itself; the user's choice is kept for the next mode change
      remote_state[2] &= ~WHIRLPOOL_FAN_MASK;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      if ((remote_state[2] & WHIRLPOOL_FAN_MASK) == WHIRLPOOL_FAN_AUTO) {
        remote_state[2] |= WHIRLPOOL_FAN_MED;
      }
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      // Auto sends the target as one of four range codes in byte 2 bits 6:4 instead of the temperature nibble.
      // Taken from the HPA-4911 firmware: 16 to 21 C, 22 C, 24 C, and everything else (23 C and 25 C up) share a code.
      if (temp_step < 6) {
        remote_state[2] |= 0x30;
      } else if (temp_step == 6) {
        remote_state[2] |= 0x50;
      } else if (temp_step == 8) {
        remote_state[2] |= 0x40;
      } else {
        remote_state[2] |= 0x20;
      }
      break;
    default:
      break;
  }
#endif
}

bool WhirlpoolClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Check if the esp isn't currently transmitting.
  if (millis() - this->last_transmit_time_ < 500) {
    ESP_LOGV(TAG, "Blocked receive because of current trasmittion");
    return false;
  }

  // Validate header
  if (!data.expect_item(WHIRLPOOL_HEADER_MARK, WHIRLPOOL_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[WHIRLPOOL_STATE_LENGTH] = {0};
  bool skip_footer = false;
  // Read all bytes.
  for (int i = 0; i < WHIRLPOOL_STATE_LENGTH; i++) {
    if (i == 6 || i == 14) {
      if (!data.expect_item(WHIRLPOOL_BIT_MARK, WHIRLPOOL_GAP))
        return false;
    }
    if (i == 14 && !data.is_valid()) {
      // Remote control only sent 14 bytes, nothing more to read, not even the footer
      ESP_LOGV(TAG, "Remote control only sent %d bytes", i);
      skip_footer = true;
      break;
    }
    if (!data.expect_bytes_lsb(remote_state + i, 1, WHIRLPOOL_BIT_MARK, WHIRLPOOL_ONE_SPACE, WHIRLPOOL_ZERO_SPACE))
      return false;
  }
  // Validate footer
  if (!data.expect_mark(WHIRLPOOL_BIT_MARK) && !skip_footer) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // Calculate  checksum and compare with signal value.
  uint8_t checksum13 = xor_bytes(&remote_state[2], 11);
  uint8_t checksum20 = xor_bytes(&remote_state[14], 6);

  if (checksum13 != remote_state[13] || (!skip_footer && checksum20 != remote_state[20])) {
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
  ESP_LOGV(TAG, "Power: %02X", (remote_state[2] & WHIRLPOOL_POWER));

  if ((remote_state[2] & WHIRLPOOL_POWER) == WHIRLPOOL_POWER) {
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
      case WHIRLPOOL_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case WHIRLPOOL_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case WHIRLPOOL_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case WHIRLPOOL_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case WHIRLPOOL_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  // Set received temp; Hisense ARG sends fixed values in auto and fan-only
  const bool hisense_arg = this->is_hisense_arg_();
  if (!hisense_arg || !this->fixed_temp_mode_()) {
    int temp = remote_state[3] & 0xF0;
    ESP_LOGVV(TAG, "Temperature Raw: %02X", temp);
    temp = (uint8_t) temp >> 4;
    temp += static_cast<int>(this->temperature_min_());
    ESP_LOGVV(TAG, "Temperature Climate: %u", temp);
    this->target_temperature = temp;
  }

  // Set received fan speed
  auto fan = remote_state[2] & WHIRLPOOL_FAN_MASK;
  ESP_LOGVV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case WHIRLPOOL_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case WHIRLPOOL_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case WHIRLPOOL_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case WHIRLPOOL_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Set received swing status
  if (hisense_arg) {
    this->swing_mode =
        (remote_state[8] & WHIRLPOOL_SWING_BYTE8) ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  } else if ((remote_state[2] & WHIRLPOOL_SWING_MASK) == WHIRLPOOL_SWING_MASK &&
             remote_state[8] == WHIRLPOOL_SWING_BYTE8) {
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

}  // namespace esphome::whirlpool
