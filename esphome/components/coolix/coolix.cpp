#include "coolix.h"
#include "esphome/components/remote_base/coolix_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace coolix {

static const char *const TAG = "coolix.climate";

static const uint32_t COOLIX_OFF = 0xB27BE0;
static const uint32_t COOLIX_SWING = 0xB26BE0;
static const uint32_t COOLIX_LED = 0xB5F5A5;
static const uint32_t COOLIX_SILENCE_FP = 0xB5F5B6;

// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
static const uint8_t COOLIX_COOL = 0b0000;
static const uint8_t COOLIX_DRY_FAN = 0b0100;
static const uint8_t COOLIX_AUTO = 0b1000;
static const uint8_t COOLIX_HEAT = 0b1100;
static const uint32_t COOLIX_MODE_MASK = 0b1100;
static const uint32_t COOLIX_FAN_MASK = 0xF000;
static const uint32_t COOLIX_FAN_MODE_AUTO_DRY = 0x1000;
static const uint32_t COOLIX_FAN_AUTO = 0xB000;
static const uint32_t COOLIX_FAN_MIN = 0x9000;
static const uint32_t COOLIX_FAN_MED = 0x5000;
static const uint32_t COOLIX_FAN_MAX = 0x3000;

// Temperature
static const uint8_t COOLIX_TEMP_RANGE = COOLIX_TEMP_MAX - COOLIX_TEMP_MIN + 1;
static const uint8_t COOLIX_FAN_TEMP_CODE = 0b11100000;  // Part of Fan Mode.
static const uint32_t COOLIX_TEMP_MASK = 0b11110000;
static const uint8_t COOLIX_TEMP_MAP[COOLIX_TEMP_RANGE] = {
    0b00000000,  // 17C
    0b00010000,  // 18c
    0b00110000,  // 19C
    0b00100000,  // 20C
    0b01100000,  // 21C
    0b01110000,  // 22C
    0b01010000,  // 23C
    0b01000000,  // 24C
    0b11000000,  // 25C
    0b11010000,  // 26C
    0b10010000,  // 27C
    0b10000000,  // 28C
    0b10100000,  // 29C
    0b10110000   // 30C
};

void CoolixClimate::transmit_state() {
  uint32_t remote_state = 0xB20F00;

  if (send_swing_cmd_) {
    send_swing_cmd_ = false;
    remote_state = COOLIX_SWING;
  } else {
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        remote_state |= COOLIX_COOL;
        break;
      case climate::CLIMATE_MODE_HEAT:
        remote_state |= COOLIX_HEAT;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        remote_state |= COOLIX_AUTO;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
      case climate::CLIMATE_MODE_DRY:
        remote_state |= COOLIX_DRY_FAN;
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        remote_state = COOLIX_OFF;
        break;
    }
    if (this->mode != climate::CLIMATE_MODE_OFF) {
      if (this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
        auto temp = (uint8_t) roundf(clamp<float>(this->target_temperature, COOLIX_TEMP_MIN, COOLIX_TEMP_MAX));
        remote_state |= COOLIX_TEMP_MAP[temp - COOLIX_TEMP_MIN];
      } else {
        remote_state |= COOLIX_FAN_TEMP_CODE;
      }
      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL || this->mode == climate::CLIMATE_MODE_DRY) {
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        remote_state |= COOLIX_FAN_MODE_AUTO_DRY;
      } else {
        switch (this->fan_mode.value()) {
          case climate::CLIMATE_FAN_HIGH:
            remote_state |= COOLIX_FAN_MAX;
            break;
          case climate::CLIMATE_FAN_MEDIUM:
            remote_state |= COOLIX_FAN_MED;
            break;
          case climate::CLIMATE_FAN_LOW:
            remote_state |= COOLIX_FAN_MIN;
            break;
          case climate::CLIMATE_FAN_AUTO:
          default:
            remote_state |= COOLIX_FAN_AUTO;
            break;
        }
      }
    }
  }
  ESP_LOGV(TAG, "Sending coolix code: 0x%06X", remote_state);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  remote_base::CoolixProtocol().encode(data, remote_state);
  transmit.perform();
}

bool CoolixClimate::on_coolix(climate::Climate *parent, remote_base::RemoteReceiveData data) {
  auto decoded = remote_base::CoolixProtocol().decode(data);
  if (!decoded.has_value())
    return false;
  // Decoded remote state y 3 bytes long code.
  uint32_t remote_state = *decoded;
  ESP_LOGV(TAG, "Decoded 0x%06X", remote_state);
  if ((remote_state & 0xFF0000) != 0xB20000)
    return false;

  if (remote_state == COOLIX_OFF) {
    parent->mode = climate::CLIMATE_MODE_OFF;
  } else if (remote_state == COOLIX_SWING) {
    parent->swing_mode =
        parent->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  } else {
    if ((remote_state & COOLIX_MODE_MASK) == COOLIX_HEAT) {
      parent->mode = climate::CLIMATE_MODE_HEAT;
    } else if ((remote_state & COOLIX_MODE_MASK) == COOLIX_AUTO) {
      parent->mode = climate::CLIMATE_MODE_HEAT_COOL;
    } else if ((remote_state & COOLIX_MODE_MASK) == COOLIX_DRY_FAN) {
      if ((remote_state & COOLIX_FAN_MASK) == COOLIX_FAN_MODE_AUTO_DRY) {
        parent->mode = climate::CLIMATE_MODE_DRY;
      } else {
        parent->mode = climate::CLIMATE_MODE_FAN_ONLY;
      }
    } else
      parent->mode = climate::CLIMATE_MODE_COOL;

    // Fan Speed
    if ((remote_state & COOLIX_FAN_AUTO) == COOLIX_FAN_AUTO || parent->mode == climate::CLIMATE_MODE_HEAT_COOL ||
        parent->mode == climate::CLIMATE_MODE_DRY) {
      parent->fan_mode = climate::CLIMATE_FAN_AUTO;
    } else if ((remote_state & COOLIX_FAN_MIN) == COOLIX_FAN_MIN) {
      parent->fan_mode = climate::CLIMATE_FAN_LOW;
    } else if ((remote_state & COOLIX_FAN_MED) == COOLIX_FAN_MED) {
      parent->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    } else if ((remote_state & COOLIX_FAN_MAX) == COOLIX_FAN_MAX) {
      parent->fan_mode = climate::CLIMATE_FAN_HIGH;
    }

    // Temperature
    uint8_t temperature_code = remote_state & COOLIX_TEMP_MASK;
    for (uint8_t i = 0; i < COOLIX_TEMP_RANGE; i++) {
      if (COOLIX_TEMP_MAP[i] == temperature_code)
        parent->target_temperature = i + COOLIX_TEMP_MIN;
    }
  }
  parent->publish_state();

  return true;
}

}  // namespace coolix
}  // namespace esphome
