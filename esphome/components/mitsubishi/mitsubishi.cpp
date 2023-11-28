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
const uint8_t MITSUBISHI_FAN = 0x38;
const uint8_t MITSUBISHI_FAN_AUTO = 0x00;

// Mitsubishi vertical swing codes
const uint8_t MITSUBISHI_VERT_SWING = 0x78;
const uint8_t MITSUBISHI_VS_AUTO = 0x40;
const uint8_t MITSUBISHI_ECONOCOOL = 0x20;  // KJ byte 14 + 0x03
const uint8_t MITSUBISHI_ISAVE = 0x20;      // KJ byte 15 w temperature=0
const uint8_t MITSUBISHI_1FLOW = 0x02;      // KJ byte 16
const uint8_t MITSUBISHI_VERT_UP = 0x48;
const uint8_t MITSUBISHI_VERT_MUP = 0x50;
const uint8_t MITSUBISHI_VERT_MIDDLE = 0x58;
const uint8_t MITSUBISHI_VERT_MDOWN = 0x60;
const uint8_t MITSUBISHI_VERT_DOWN = 0x68;

const uint8_t MITSUBISHI_HORZ_SWING = 0xC0;
const uint8_t MITSUBISHI_HORZ_MIDDLE = 0x30;
const uint8_t MITSUBISHI_HORZ_LEFT = 0x10;
const uint8_t MITSUBISHI_HORZ_MLEFT = 0x20;
const uint8_t MITSUBISHI_HORZ_MRIGHT = 0x40;
const uint8_t MITSUBISHI_HORZ_RIGHT = 0x50;

// Pulse parameters in usec
const uint16_t MITSUBISHI_BIT_MARK = 430;
const uint16_t MITSUBISHI_ONE_SPACE = 1250;
const uint16_t MITSUBISHI_ZERO_SPACE = 390;
const uint16_t MITSUBISHI_HEADER_MARK = 3500;
const uint16_t MITSUBISHI_HEADER_SPACE = 1700;
const uint16_t MITSUBISHI_MIN_GAP = 17500;

void MitsubishiClimate::transmit_state() {
  uint32_t remote_state[18] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x08, 0x00, 0x30,
                               0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
    case climate::CLIMATE_MODE_DRY:
      remote_state[6] = MITSUBISHI_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[6] = MITSUBISHI_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] = MITSUBISHI_OFF;
      break;
  }

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      remote_state[9] = 5;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[9] = 1;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      remote_state[9] = 2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[9] = 3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[9] = 4;
      break;

    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[9] = 0;
      break;
  }

  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      remote_state[9] |= MITSUBISHI_VERT_SWING;
      remote_state[8] = MITSUBISHI_HORZ_MIDDLE;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      remote_state[8] = MITSUBISHI_HORZ_SWING;
      remote_state[9] |= MITSUBISHI_VERT_MIDDLE;
      break;
    case climate::CLIMATE_SWING_BOTH:
      remote_state[8] = MITSUBISHI_HORZ_SWING;
      remote_state[9] |= MITSUBISHI_VERT_SWING;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      remote_state[8] = MITSUBISHI_HORZ_MIDDLE;
      remote_state[9] |= MITSUBISHI_VERT_MIDDLE;
      break;
  }

  switch (this->preset.value()) {
    case climate::CLIMATE_PRESET_ECO:
      /* My unit doesn't support this, so I can't test it */
      remote_state[9] = 0x7 | MITSUBISHI_ECONOCOOL;
      break;
    case climate::CLIMATE_PRESET_BOOST:
      /* My unit doesn't support this, so I can't test it */
      remote_state[9] = (remote_state[9] & ~0x07) | 4;
      break;
    case climate::CLIMATE_PRESET_NONE:
    default:
      break;
  }

  remote_state[7] = (uint8_t) roundf(clamp<float>(this->target_temperature, MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX) -
                                     MITSUBISHI_TEMP_MIN);

  ESP_LOGV(TAG,
           "Sending Mitsubishi target temp: %.1f state: %02" PRIX32 " mode: %02" PRIX32 " temp: %02" PRIX32
           " fan/vswing: %02" PRIX32 " hswing: %02" PRIX32,
           this->target_temperature, remote_state[5], remote_state[6], remote_state[7], remote_state[9],
           remote_state[8]);

  // Checksum
  for (int i = 0; i < 17; i++) {
    remote_state[17] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

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
