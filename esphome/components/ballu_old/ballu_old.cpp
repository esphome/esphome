#include "ballu_old.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ballu_old {

static const char *const TAG = "ballu_old.climate";

const uint16_t BALLU_HEADER_MARK = 3000;
const uint16_t BALLU_HEADER_SPACE = 1500;
const uint16_t BALLU_BIT_MARK = 500;
const uint16_t BALLU_ONE_SPACE = 1000;
const uint16_t BALLU_ZERO_SPACE = 350;

const uint32_t BALLU_CARRIER_FREQUENCY = 38000;

const uint8_t BALLU_STATE_LENGTH = 14;

const uint8_t BALLU_AUTO = 0x08;
const uint8_t BALLU_COOL = 0x03;
const uint8_t BALLU_DRY = 0x02;
const uint8_t BALLU_HEAT = 0x01;
const uint8_t BALLU_FAN = 0x07;

const uint8_t BALLU_FAN_AUTO = 0x00;
const uint8_t BALLU_FAN_HIGH = 0x05;
const uint8_t BALLU_FAN_MED = 0x03;
const uint8_t BALLU_FAN_LOW = 0x02;

const uint8_t BALLU_POWER = 0x04;
const uint8_t BALLU_SWING = 0x38;

void BalluOldClimate::transmit_state() {
  uint8_t remote_state[BALLU_STATE_LENGTH] = {0};

  auto temp = (uint8_t) roundf(clamp(this->target_temperature, YKR_K_002E_TEMP_MIN, YKR_K_002E_TEMP_MAX));
  auto swing_mode = this->swing_mode != climate::CLIMATE_SWING_OFF;

  remote_state[0] = 0x23;
  remote_state[1] = 0xCB;
  remote_state[2] = 0x26;
  remote_state[3] = 0x01;
  remote_state[5] = (this->mode == climate::CLIMATE_MODE_OFF) ? 0 : BALLU_POWER | (0x01 << 5);
  remote_state[7] = 0x1F - temp;
  remote_state[8] = (this->swing_mode == climate::CLIMATE_SWING_OFF) ? 0x00 : BALLU_SWING;

  // Fan speed
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[8] |= BALLU_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[8] |= BALLU_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[8] |= BALLU_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_AUTO:
      remote_state[8] |= BALLU_FAN_AUTO;
      break;
    default:
      break;
  }

  // Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
      remote_state[6] |= BALLU_AUTO;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] |= BALLU_HEAT;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] |= BALLU_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[6] |= BALLU_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[6] |= BALLU_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
      remote_state[6] |= BALLU_AUTO;
    default:
      break;
  }

  // Checksum
  for (uint8_t i = 0; i < BALLU_STATE_LENGTH - 1; i++)
    remote_state[13] += remote_state[i];

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X", remote_state[0],
           remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6],
           remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11], remote_state[12]);

  // Send code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(BALLU_HEADER_MARK);
  data->space(BALLU_HEADER_SPACE);
  // Data
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(BALLU_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? BALLU_ONE_SPACE : BALLU_ZERO_SPACE);
    }
  }
  // Footer
  data->mark(BALLU_BIT_MARK);

  transmit.perform();
}

}  // namespace ballu_old
}  // namespace esphome
