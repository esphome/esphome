#include "islandaire.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::islandaire {

static const char *const TAG = "islandaire.climate";

// 14-byte frame, LSB first:
//   Bytes 0-4: 0x22 preamble
//   Byte 5:    power (0x20 off, 0x24 on); bit 3 (0x08) set while the auto-off timer is active
//   Byte 6:    mode (0x01 heat, 0x03 cool, 0x04 fan only)
//   Byte 7:    target temperature in °F (61-88)
//   Byte 8:    fan speed (0x00 auto, 0x02 low, 0x04 high); bit 6 (0x40) set while the timer is active
//   Byte 9:    0x00
//   Byte 10:   auto-off timer in hours (1-12, 0 = off)
//   Bytes 11-12: 0x00
//   Byte 13:   sum of bytes 0-12
// The auto-off timer is not implemented: the unit switches itself off with no feedback, which
// would leave the climate state stale.
const uint16_t ISLANDAIRE_STATE_LENGTH = 14;
const uint8_t ISLANDAIRE_PREAMBLE_LENGTH = 5;
const uint8_t ISLANDAIRE_PREAMBLE = 0x22;

const uint8_t ISLANDAIRE_POWER_OFF = 0x20;
const uint8_t ISLANDAIRE_POWER_ON = 0x24;
const uint8_t ISLANDAIRE_POWER_MASK = 0x04;

const uint8_t ISLANDAIRE_HEAT = 0x01;
const uint8_t ISLANDAIRE_COOL = 0x03;
const uint8_t ISLANDAIRE_FAN = 0x04;

const uint8_t ISLANDAIRE_FAN_AUTO = 0x00;
const uint8_t ISLANDAIRE_FAN_LOW = 0x02;
const uint8_t ISLANDAIRE_FAN_HIGH = 0x04;

const uint8_t ISLANDAIRE_FAN_TIMER_FLAG = 0x40;  // Byte 8 Bit 6: Set when Timer is active

const uint8_t ISLANDAIRE_TEMP_MIN_F = 61;
const uint8_t ISLANDAIRE_TEMP_MAX_F = 88;

const uint16_t ISLANDAIRE_HEADER_MARK = 3608;
const uint16_t ISLANDAIRE_HEADER_SPACE = 1520;
const uint16_t ISLANDAIRE_BIT_MARK = 560;
const uint16_t ISLANDAIRE_ONE_SPACE = 1208;
const uint16_t ISLANDAIRE_ZERO_SPACE = 408;

static uint8_t calc_checksum(const uint8_t *state) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < ISLANDAIRE_STATE_LENGTH - 1; i++)
    sum += state[i];
  return sum;
}

void IslandaireClimate::transmit_state() {
  uint8_t remote_state[ISLANDAIRE_STATE_LENGTH] = {0};

  for (uint8_t i = 0; i < ISLANDAIRE_PREAMBLE_LENGTH; i++)
    remote_state[i] = ISLANDAIRE_PREAMBLE;

  remote_state[5] = this->mode == climate::CLIMATE_MODE_OFF ? ISLANDAIRE_POWER_OFF : ISLANDAIRE_POWER_ON;

  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] = ISLANDAIRE_HEAT;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[6] = ISLANDAIRE_FAN;
      break;
    case climate::CLIMATE_MODE_COOL:
    default:
      remote_state[6] = ISLANDAIRE_COOL;
      break;
  }

  // Target temperature is sent as whole degrees Fahrenheit.
  auto temp_f = (uint8_t) std::round(this->target_temperature * 1.8f + 32.0f);
  remote_state[7] = std::clamp(temp_f, ISLANDAIRE_TEMP_MIN_F, ISLANDAIRE_TEMP_MAX_F);

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      remote_state[8] = ISLANDAIRE_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[8] = ISLANDAIRE_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[8] = ISLANDAIRE_FAN_AUTO;
      break;
  }

  remote_state[ISLANDAIRE_STATE_LENGTH - 1] = calc_checksum(remote_state);

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X", remote_state[0],
           remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6],
           remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11], remote_state[12],
           remote_state[13]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(ISLANDAIRE_HEADER_MARK);
  data->space(ISLANDAIRE_HEADER_SPACE);
  // Data (LSB first)
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(ISLANDAIRE_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? ISLANDAIRE_ONE_SPACE : ISLANDAIRE_ZERO_SPACE);
    }
  }
  // Footer
  data->mark(ISLANDAIRE_BIT_MARK);

  transmit.perform();
}

bool IslandaireClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(ISLANDAIRE_HEADER_MARK, ISLANDAIRE_HEADER_SPACE)) {
    ESP_LOGVV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[ISLANDAIRE_STATE_LENGTH] = {0};
  // Read all bytes (LSB first)
  for (int i = 0; i < ISLANDAIRE_STATE_LENGTH; i++) {
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(ISLANDAIRE_BIT_MARK, ISLANDAIRE_ONE_SPACE)) {
        remote_state[i] |= 1 << j;
      } else if (!data.expect_item(ISLANDAIRE_BIT_MARK, ISLANDAIRE_ZERO_SPACE)) {
        ESP_LOGVV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }
  }
  if (!data.expect_mark(ISLANDAIRE_BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  if (calc_checksum(remote_state) != remote_state[ISLANDAIRE_STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  for (uint8_t i = 0; i < ISLANDAIRE_PREAMBLE_LENGTH; i++) {
    if (remote_state[i] != ISLANDAIRE_PREAMBLE)
      return false;
  }

  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
           remote_state[12], remote_state[13]);

  if ((remote_state[5] & ISLANDAIRE_POWER_MASK) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (remote_state[6]) {
      case ISLANDAIRE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case ISLANDAIRE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case ISLANDAIRE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        ESP_LOGV(TAG, "Unknown mode 0x%02X", remote_state[6]);
        return false;
    }
  }

  uint8_t temp_f = remote_state[7];
  if (temp_f >= ISLANDAIRE_TEMP_MIN_F && temp_f <= ISLANDAIRE_TEMP_MAX_F)
    this->target_temperature = (temp_f - 32.0f) / 1.8f;

  // Byte 8: Fan speed (Bit 6 is timer active indicator)
  uint8_t fan = remote_state[8] & ~ISLANDAIRE_FAN_TIMER_FLAG;
  switch (fan) {
    case ISLANDAIRE_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case ISLANDAIRE_FAN_LOW:
    case 0x01:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case ISLANDAIRE_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::islandaire
