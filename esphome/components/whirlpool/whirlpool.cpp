#include "whirlpool.h"
#include "esphome/core/log.h"

namespace esphome {
namespace whirlpool {

static const char *TAG = "whirlpool.climate";

const uint16_t WHIRLPOOL_HEADER_MARK = 9000;
const uint16_t WHIRLPOOL_HEADER_SPACE = 4494;
const uint16_t WHIRLPOOL_BIT_MARK = 572;
const uint16_t WHIRLPOOL_ONE_SPACE = 1659;
const uint16_t WHIRLPOOL_ZERO_SPACE = 553;
const uint32_t WHIRLPOOL_GAP = 7960;

const uint32_t WHIRLPOOL_CARRIER_FREQUENCY = 38000;

const uint8_t WHIRLPOOL_STATE_LENGTH = 21;

void WhirlpoolClimate::transmit_state() {
  uint8_t remote_state[WHIRLPOOL_STATE_LENGTH] = {0};
  remote_state[0] = 0x83;
  remote_state[1] = 0x06;
  remote_state[6] = 0x80;
  // MODEL DG11J191
  remote_state[18] = 0x08;

  auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;
  if (powered_on != this->powered_on_assumed_) {
    // Set power toggle command
    remote_state[2] = 4;
    remote_state[15] = 1;
    this->powered_on_assumed_ = powered_on;
  }
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
      // set fan auto
      // set temp auto temp
      // set sleep false
      remote_state[3] = 1;
      remote_state[15] = 0x17;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[3] = 0;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[3] = 2;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[3] = 3;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[3] = 4;
      remote_state[15] = 6;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  // Temperature
  auto temp = (uint8_t) roundf(clamp(this->target_temperature, WHIRLPOOL_TEMP_MIN, WHIRLPOOL_TEMP_MAX));
  remote_state[3] |= (uint8_t)(temp - WHIRLPOOL_TEMP_MIN) << 4;

  // Fan speed
  switch (this->fan_mode) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state[2] |= 1;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[2] |= 2;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[2] |= 3;
      break;
  }

  // Swing
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    remote_state[2] |= 128;
    remote_state[8] |= 64;
  }

  // Checksum
  for (uint8_t i = 2; i < 12; i++)
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
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(WHIRLPOOL_HEADER_MARK);
  data->space(WHIRLPOOL_HEADER_SPACE);
  // Data
  auto bytes_sent = 0;
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(WHIRLPOOL_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? WHIRLPOOL_ONE_SPACE : WHIRLPOOL_ZERO_SPACE);
    }
    bytes_sent++;
    if (bytes_sent == 6 || bytes_sent == 14) {
      // Divider
      data->mark(WHIRLPOOL_BIT_MARK);
      data->space(WHIRLPOOL_GAP);
    }
  }
  // Footer
  data->mark(WHIRLPOOL_BIT_MARK);

  transmit.perform();
}

}  // namespace whirlpool
}  // namespace esphome
