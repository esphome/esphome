#include "islandaire.h"
#include "esphome/components/climate_ir/climate_ir.h"
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
static constexpr remote_base::BitTimings BIT_TIMINGS{
    .bit_mark = BIT_MARK, .one_space = ONE_SPACE, .zero_space = ZERO_SPACE};

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT, MODE_HEAT},
    {climate::CLIMATE_MODE_COOL, MODE_COOL},
    {climate::CLIMATE_MODE_FAN_ONLY, MODE_FAN},
};
// The second low code is only received
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, FAN_LOW},
    {climate::CLIMATE_FAN_HIGH, FAN_HIGH},
    {climate::CLIMATE_FAN_LOW, FAN_LOW_ALT},
};

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

  remote_state[6] = to_ir_code(MODES, this->mode, MODE_COOL);

  // Whole degrees within the unit's range, stored as well so the published state matches what was sent
  const auto temperature =
      static_cast<uint8_t>(std::clamp<float>(std::round(this->target_temperature), TEMP_MIN, TEMP_MAX));
  this->target_temperature = temperature;
  remote_state[7] = temperature;

  remote_state[8] = to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), FAN_AUTO);

  remote_state[STATE_LENGTH - 1] = sum_bytes(remote_state, STATE_LENGTH - 1);

  ESP_LOGV_HEX(TAG, remote_state, STATE_LENGTH, STATE_LENGTH, "Sending: %s");

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(CARRIER_FREQUENCY);

  data->reserve(2 + STATE_LENGTH * 16 + 1);
  data->item(HEADER_MARK, HEADER_SPACE);
  data->item_bytes<BIT_TIMINGS>(remote_state, STATE_LENGTH);
  data->mark(BIT_MARK);

  transmit.perform();
}

bool IslandaireClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE)) {
    ESP_LOGVV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[STATE_LENGTH];
  if (!data.expect_bytes<BIT_TIMINGS>(remote_state, STATE_LENGTH)) {
    ESP_LOGVV(TAG, "Data fail");
    return false;
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
  } else if (const auto entry = find_ir_code(MODES, remote_state[6], true)) {
    this->mode = static_cast<climate::ClimateMode>(entry->value);
  } else {
    ESP_LOGV(TAG, "Unknown mode 0x%02X", remote_state[6]);
    return false;
  }

  const uint8_t temp_f = remote_state[7];
  if (temp_f >= TEMP_MIN && temp_f <= TEMP_MAX)
    this->target_temperature = temp_f;

  this->fan_mode = from_ir_code(FANS, remote_state[8] & ~FAN_TIMER_FLAG, climate::CLIMATE_FAN_AUTO);

  this->publish_state();
  return true;
}

}  // namespace esphome::islandaire
