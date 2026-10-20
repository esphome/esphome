#include "islandaire.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::islandaire {

ESPHOME_LOG_TAG(TAG, "islandaire.climate");

// 14-byte frame, LSB first:
//   Bytes 0-4: 0x22 preamble
//   Byte 5:    power (0x20 off, 0x24 on); bit 3 (0x08) set while the auto-off timer is active
//   Byte 6:    mode (0x01 heat, 0x03 cool, 0x04 fan only)
//   Byte 7:    target temperature in °F (61-88)
//   Byte 8:    fan speed (0x00 auto, 0x02 low, 0x04 high, 0x01 read as low too); bit 6 (0x40) set while the
//              timer is active
//   Byte 9:    0x00
//   Byte 10:   auto-off timer in hours (1-12, 0 = off)
//   Bytes 11-12: 0x00
//   Byte 13:   sum of bytes 0-12
// The auto-off timer is not implemented: the unit switches itself off with no feedback, which
// would leave the climate state stale.
static constexpr uint8_t STATE_LENGTH = 14;
static constexpr uint8_t PREAMBLE_LENGTH = 5;
static constexpr uint8_t PREAMBLE = 0x22;

static constexpr uint8_t POWER_OFF = 0x20;
static constexpr uint8_t POWER_MASK = 0x04;
static constexpr uint8_t POWER_ON = POWER_OFF | POWER_MASK;

static constexpr uint8_t MODE_HEAT = 0x01;
static constexpr uint8_t MODE_COOL = 0x03;
static constexpr uint8_t MODE_FAN = 0x04;

static constexpr uint8_t FAN_AUTO = 0x00;
static constexpr uint8_t FAN_LOW_ALT = 0x01;
static constexpr uint8_t FAN_LOW = 0x02;
static constexpr uint8_t FAN_HIGH = 0x04;
static constexpr uint8_t FAN_TIMER_FLAG = 0x40;

static constexpr uint8_t TEMP_DEFAULT = 72;

static constexpr uint16_t HEADER_MARK = 3608;
static constexpr uint16_t HEADER_SPACE = 1520;
static constexpr uint16_t BIT_MARK = 560;
static constexpr uint16_t ONE_SPACE = 1208;
static constexpr uint16_t ZERO_SPACE = 408;
static constexpr uint32_t CARRIER_FREQUENCY = 38000;

void IslandaireClimate::setup() {
  ClimateIR::setup();
  // The base class default assumes Celsius
  if (std::isnan(this->target_temperature) || this->target_temperature < TEMP_MIN)
    this->target_temperature = TEMP_DEFAULT;
  this->target_temperature = std::min<float>(this->target_temperature, TEMP_MAX);
}

void IslandaireClimate::dump_config() {
  // ClimateIR::dump_config() is not called: it prints the temperature range in Celsius
  LOG_CLIMATE("", "Islandaire Climate", this);
  ESP_LOGCONFIG(TAG, "  Temperature range: %u to %u °F", TEMP_MIN, TEMP_MAX);
}

climate::ClimateTraits IslandaireClimate::traits() {
  auto traits = ClimateIR::traits();
  traits.set_temperature_unit(TemperatureUnit::FAHRENHEIT);
  return traits;
}

void IslandaireClimate::transmit_state() {
  uint8_t remote_state[STATE_LENGTH] = {0};

  std::fill_n(remote_state, PREAMBLE_LENGTH, PREAMBLE);

  remote_state[5] = this->mode == climate::CLIMATE_MODE_OFF ? POWER_OFF : POWER_ON;

  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[6] = MODE_FAN;
      break;
    case climate::CLIMATE_MODE_COOL:
    default:
      remote_state[6] = MODE_COOL;
      break;
  }

  // Whole degrees within the unit's range, stored as well so the published state matches what was sent
  const auto temperature =
      static_cast<uint8_t>(std::clamp<float>(std::round(this->target_temperature), TEMP_MIN, TEMP_MAX));
  this->target_temperature = temperature;
  remote_state[7] = temperature;

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      remote_state[8] = FAN_LOW;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[8] = FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[8] = FAN_AUTO;
      break;
  }

  remote_state[STATE_LENGTH - 1] = sum_bytes(remote_state, STATE_LENGTH - 1);

  ESP_LOGV_HEX(TAG, remote_state, STATE_LENGTH, STATE_LENGTH, "Sending: %s");

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(CARRIER_FREQUENCY);

  data->reserve(2 + STATE_LENGTH * 16 + 1);
  data->item(HEADER_MARK, HEADER_SPACE);
  for (uint8_t byte : remote_state) {
    for (uint8_t bit = 0; bit < 8; bit++)
      data->item(BIT_MARK, byte & (1 << bit) ? ONE_SPACE : ZERO_SPACE);
  }
  data->mark(BIT_MARK);

  transmit.perform();
}

bool IslandaireClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE)) {
    ESP_LOGVV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[STATE_LENGTH] = {0};
  for (uint8_t byte = 0; byte < STATE_LENGTH; byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(BIT_MARK, ONE_SPACE)) {
        remote_state[byte] |= 1 << bit;
      } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
        ESP_LOGVV(TAG, "Byte %d bit %d fail", byte, bit);
        return false;
      }
    }
  }
  if (!data.expect_mark(BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  if (sum_bytes(remote_state, STATE_LENGTH - 1) != remote_state[STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  for (uint8_t i = 0; i < PREAMBLE_LENGTH; i++) {
    if (remote_state[i] != PREAMBLE)
      return false;
  }

  ESP_LOGV_HEX(TAG, remote_state, STATE_LENGTH, STATE_LENGTH, "Received: %s");

  if ((remote_state[5] & POWER_MASK) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (remote_state[6]) {
      case MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        ESP_LOGV(TAG, "Unknown mode 0x%02X", remote_state[6]);
        return false;
    }
  }

  const uint8_t temp_f = remote_state[7];
  if (temp_f >= TEMP_MIN && temp_f <= TEMP_MAX)
    this->target_temperature = temp_f;

  switch (remote_state[8] & ~FAN_TIMER_FLAG) {
    case FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case FAN_LOW:
    case FAN_LOW_ALT:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::islandaire
