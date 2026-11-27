#include "ballu.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ballu {

ESPHOME_LOG_TAG(TAG, "ballu.climate");

const uint16_t BALLU_HEADER_MARK = 9000;
const uint16_t BALLU_HEADER_SPACE = 4500;
const uint16_t BALLU_BIT_MARK = 575;
static constexpr remote_base::BitTimings BALLU_BIT_TIMINGS{
    .bit_mark = BALLU_BIT_MARK, .one_space = 1675, .zero_space = 550};

const uint32_t BALLU_CARRIER_FREQUENCY = 38000;

const uint8_t BALLU_STATE_LENGTH = 13;

const uint8_t BALLU_AUTO = 0;
const uint8_t BALLU_COOL = 0x20;
const uint8_t BALLU_DRY = 0x40;
const uint8_t BALLU_HEAT = 0x80;
const uint8_t BALLU_FAN = 0xc0;

const uint8_t BALLU_FAN_AUTO = 0xa0;
const uint8_t BALLU_FAN_HIGH = 0x20;
const uint8_t BALLU_FAN_MED = 0x40;
const uint8_t BALLU_FAN_LOW = 0x60;

const uint8_t BALLU_SWING_VER = 0x07;
const uint8_t BALLU_SWING_HOR = 0xe0;
const uint8_t BALLU_POWER = 0x20;

using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// A fan mode not listed sends no fan bits and a received mode not listed is ignored
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_AUTO, BALLU_AUTO},    {climate::CLIMATE_MODE_HEAT, BALLU_HEAT},
    {climate::CLIMATE_MODE_COOL, BALLU_COOL},    {climate::CLIMATE_MODE_DRY, BALLU_DRY},
    {climate::CLIMATE_MODE_FAN_ONLY, BALLU_FAN},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, BALLU_FAN_AUTO},
    {climate::CLIMATE_FAN_HIGH, BALLU_FAN_HIGH},
    {climate::CLIMATE_FAN_MEDIUM, BALLU_FAN_MED},
    {climate::CLIMATE_FAN_LOW, BALLU_FAN_LOW},
};

void BalluClimate::transmit_state() {
  uint8_t remote_state[BALLU_STATE_LENGTH] = {0};

  auto temp = (uint8_t) roundf(clamp(this->target_temperature, YKR_K_002E_TEMP_MIN, YKR_K_002E_TEMP_MAX));
  auto swing_ver =
      ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH));
  auto swing_hor =
      ((this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH));

  remote_state[0] = 0xc3;
  remote_state[1] = ((temp - 8) << 3) | (swing_ver ? 0 : BALLU_SWING_VER);
  remote_state[2] = swing_hor ? 0 : BALLU_SWING_HOR;
  remote_state[9] = (this->mode == climate::CLIMATE_MODE_OFF) ? 0 : BALLU_POWER;
  remote_state[11] = 0x1e;

  remote_state[4] = to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_ON), 0);
  remote_state[6] = to_ir_code(MODES, this->mode, BALLU_AUTO);

  // Checksum
  for (uint8_t i = 0; i < BALLU_STATE_LENGTH - 1; i++)
    remote_state[12] += remote_state[i];

  ESP_LOGV_HEX(TAG, remote_state, BALLU_STATE_LENGTH, BALLU_STATE_LENGTH, "Sending: %s");

  // Send code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(BALLU_HEADER_MARK);
  data->space(BALLU_HEADER_SPACE);
  // Data
  data->item_bytes<BALLU_BIT_TIMINGS>(remote_state, BALLU_STATE_LENGTH);
  // Footer
  data->mark(BALLU_BIT_MARK);

  transmit.perform();
}

bool BalluClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header
  if (!data.expect_item(BALLU_HEADER_MARK, BALLU_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t remote_state[BALLU_STATE_LENGTH];
  if (!data.expect_bytes<BALLU_BIT_TIMINGS>(remote_state, BALLU_STATE_LENGTH)) {
    ESP_LOGV(TAG, "Data fail");
    return false;
  }
  // Validate footer
  if (!data.expect_mark(BALLU_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  const uint8_t checksum = sum_bytes(remote_state, BALLU_STATE_LENGTH - 1);

  if (checksum != remote_state[BALLU_STATE_LENGTH - 1]) {
    ESP_LOGVV(TAG, "Checksum fail");
    return false;
  }

  ESP_LOGV_HEX(TAG, remote_state, BALLU_STATE_LENGTH, BALLU_STATE_LENGTH, "Received: %s");

  // verify header remote code
  if (remote_state[0] != 0xc3)
    return false;

  // powr on/off button
  ESP_LOGV(TAG, "Power: %02X", (remote_state[9] & BALLU_POWER));

  if ((remote_state[9] & BALLU_POWER) != BALLU_POWER) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    auto mode = remote_state[6] & 0xe0;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    this->mode = from_ir_code(MODES, mode, this->mode);
  }

  // Set received temp
  int temp = remote_state[1] & 0xf8;
  ESP_LOGVV(TAG, "Temperature Raw: %02X", temp);
  temp = ((uint8_t) temp >> 3) + 8;
  ESP_LOGVV(TAG, "Temperature Climate: %u", temp);
  this->target_temperature = temp;

  // Set received fan speed
  auto fan = remote_state[4] & 0xe0;
  ESP_LOGVV(TAG, "Fan: %02X", fan);
  this->fan_mode = from_ir_code(FANS, fan, climate::CLIMATE_FAN_AUTO);

  // Set received swing status
  ESP_LOGVV(TAG, "Swing status: %02X %02X", remote_state[1] & BALLU_SWING_VER, remote_state[2] & BALLU_SWING_HOR);
  if (((remote_state[1] & BALLU_SWING_VER) != BALLU_SWING_VER) &&
      ((remote_state[2] & BALLU_SWING_HOR) != BALLU_SWING_HOR)) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if ((remote_state[1] & BALLU_SWING_VER) != BALLU_SWING_VER) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if ((remote_state[2] & BALLU_SWING_HOR) != BALLU_SWING_HOR) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::ballu
