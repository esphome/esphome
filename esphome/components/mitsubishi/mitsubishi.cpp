#include "mitsubishi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mitsubishi {

static const char *const TAG = "mitsubishi.climate";

const uint32_t MITSUBISHI_OFF = 0x00;

const uint8_t MITSUBISHI_COOL = 0x18;
const uint8_t MITSUBISHI_DRY = 0x10;
const uint8_t MITSUBISHI_AUTO = 0x20;
const uint8_t MITSUBISHI_HEAT = 0x08;

// Vane settings
const uint8_t MITSUBISHI_VANE_HIGH = 0x48;
const uint8_t MITSUBISHI_VANE_MID = 0x58;
const uint8_t MITSUBISHI_VANE_LOW = 0x68;
const uint8_t MITSUBISHI_VANE_AUTO = 0x40;
const uint8_t MITSUBISHI_VANE_MOVE = 0x78;

// Fan settings
const uint8_t MITSUBISHI_FAN_LOW = 0x01;
const uint8_t MITSUBISHI_FAN_MID = 0x02;
const uint8_t MITSUBISHI_FAN_HIGH = 0x03;
const uint8_t MITSUBISHI_FAN_MAX = 0x04;
const uint8_t MITSUBISHI_FAN_AUTO = 0x80;
const uint8_t MITSUBISHI_FAN_QUIET = 0x05;

// Pulse parameters in usec
const uint16_t MITSUBISHI_BIT_MARK = 430;
const uint16_t MITSUBISHI_ONE_SPACE = 1250;
const uint16_t MITSUBISHI_ZERO_SPACE = 390;
const uint16_t MITSUBISHI_HEADER_MARK = 3500;
const uint16_t MITSUBISHI_HEADER_SPACE = 1700;
const uint16_t MITSUBISHI_MIN_GAP = 17500;

void MitsubishiClimate::transmit_state() {
  uint32_t remote_state[18] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x08, 0x00, 0x30,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] = MITSUBISHI_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] = MITSUBISHI_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[6] = MITSUBISHI_AUTO;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] = MITSUBISHI_OFF;
      break;
  }

  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
      remote_state[9] |= MITSUBISHI_VANE_HIGH;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      remote_state[9] |= MITSUBISHI_VANE_LOW;
      break;
    case climate::CLIMATE_SWING_BOTH:
      remote_state[9] |= MITSUBISHI_VANE_MID;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      remote_state[9] |= MITSUBISHI_VANE_MOVE;
      break;
  }

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      remote_state[9] |= MITSUBISHI_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      remote_state[9] |= MITSUBISHI_FAN_MID;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[9] |= MITSUBISHI_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_FOCUS:
      remote_state[9] |= MITSUBISHI_FAN_MAX;
      break;
    case climate::CLIMATE_FAN_QUIET:
      remote_state[9] |= MITSUBISHI_FAN_QUIET;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[9] |= MITSUBISHI_FAN_AUTO;
      break;
  }

  remote_state[7] = (uint8_t) roundf(clamp<float>(this->target_temperature, MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX) -
                                     MITSUBISHI_TEMP_MIN);

  ESP_LOGI(TAG, "Sending Mitsubishi target temp: %.1f state: %02X mode: %02X temp: %02X fan/vane: %02X",
           this->target_temperature, remote_state[5], remote_state[6], remote_state[7], remote_state[9]);

  // Checksum
  for (int i = 0; i < 17; i++) {
    remote_state[17] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  // repeat twice
  for (uint16_t r = 0; r < 2; r++) {
    // Header
    data->mark(MITSUBISHI_HEADER_MARK);
    data->space(MITSUBISHI_HEADER_SPACE);
    // Data
    for (uint8_t i : remote_state) {
      for (uint8_t j = 0; j < 8; j++) {
        data->mark(MITSUBISHI_BIT_MARK);
        bool bit = i & (1 << j);
        data->space(bit ? MITSUBISHI_ONE_SPACE : MITSUBISHI_ZERO_SPACE);
      }
    }
    // Footer
    if (r == 0) {
      data->mark(MITSUBISHI_BIT_MARK);
      data->space(MITSUBISHI_MIN_GAP);  // Pause before repeating
    }
  }
  data->mark(MITSUBISHI_BIT_MARK);

  transmit.perform();
}

}  // namespace mitsubishi
}  // namespace esphome