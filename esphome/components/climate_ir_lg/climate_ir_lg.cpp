#include "climate_ir_lg.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_ir_lg {

static const char *TAG = "climate.climate_ir_lg";

const uint32_t COMMAND_ON = 0x00000;
const uint32_t COMMAND_ON_AI = 0x03000;
const uint32_t COMMAND_COOL = 0x08000;
const uint32_t COMMAND_OFF = 0xC0000;
const uint32_t COMMAND_SWING = 0x10000;
// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
const uint32_t COMMAND_AUTO = 0x0B000;
const uint32_t COMMAND_DRY_FAN = 0x09000;

const uint32_t COMMAND_MASK = 0xFF000;

const uint32_t FAN_MASK = 0xF0;
const uint32_t FAN_AUTO = 0x50;
const uint32_t FAN_MIN = 0x00;
const uint32_t FAN_MED = 0x20;
const uint32_t FAN_MAX = 0x40;

// Temperature
const uint8_t TEMP_RANGE = TEMP_MAX - TEMP_MIN + 1;
const uint32_t TEMP_MASK = 0XF00;
const uint32_t TEMP_SHIFT = 8;

// Constants
static const uint32_t HEADER_HIGH_US = 8000;
static const uint32_t HEADER_LOW_US = 4000;
static const uint32_t BIT_HIGH_US = 600;
static const uint32_t BIT_ONE_LOW_US = 1600;
static const uint32_t BIT_ZERO_LOW_US = 550;

const uint16_t BITS = 28;

void LgIrClimate::transmit_state() {
  uint32_t remote_state = 0x8800000;

  // ESP_LOGD(TAG, "climate_lg_ir mode_before_ code: 0x%02X", modeBefore_);
  if (send_swing_cmd_) {
    send_swing_cmd_ = false;
    remote_state |= COMMAND_SWING;
  } else {
    if (mode_before_ == climate::CLIMATE_MODE_OFF && this->mode == climate::CLIMATE_MODE_AUTO) {
      remote_state |= COMMAND_ON_AI;
    } else if (mode_before_ == climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_OFF) {
      remote_state |= COMMAND_ON;
      this->mode = climate::CLIMATE_MODE_COOL;
    } else {
      switch (this->mode) {
        case climate::CLIMATE_MODE_COOL:
          remote_state |= COMMAND_COOL;
          break;
        case climate::CLIMATE_MODE_AUTO:
          remote_state |= COMMAND_AUTO;
          break;
        case climate::CLIMATE_MODE_DRY:
          remote_state |= COMMAND_DRY_FAN;
          break;
        case climate::CLIMATE_MODE_OFF:
        default:
          remote_state |= COMMAND_OFF;
          break;
      }
    }
    mode_before_ = this->mode;

    ESP_LOGD(TAG, "climate_lg_ir mode code: 0x%02X", this->mode);

    if (this->mode == climate::CLIMATE_MODE_OFF) {
      remote_state |= FAN_AUTO;
    } else if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_DRY) {
      switch (this->fan_mode) {
        case climate::CLIMATE_FAN_HIGH:
          remote_state |= FAN_MAX;
          break;
        case climate::CLIMATE_FAN_MEDIUM:
          remote_state |= FAN_MED;
          break;
        case climate::CLIMATE_FAN_LOW:
          remote_state |= FAN_MIN;
          break;
        case climate::CLIMATE_FAN_AUTO:
        default:
          remote_state |= FAN_AUTO;
          break;
      }
    }

    if (this->mode == climate::CLIMATE_MODE_AUTO) {
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      // remote_state |= FAN_MODE_AUTO_DRY;
    }
    if (this->mode == climate::CLIMATE_MODE_COOL) {
      auto temp = (uint8_t) roundf(clamp(this->target_temperature, TEMP_MIN, TEMP_MAX));
      remote_state |= ((temp - 15) << TEMP_SHIFT);
    }
  }
  transmit_(remote_state);
  this->publish_state();
}

bool LgIrClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t nbits = 0;
  uint32_t remote_state = 0;

  if (!data.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return false;

  for (nbits = 0; nbits < 32; nbits++) {
    if (data.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      remote_state = (remote_state << 1) | 1;
    } else if (data.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      remote_state = (remote_state << 1) | 0;
    } else if (nbits == BITS) {
      break;
    } else {
      return false;
    }
  }

  ESP_LOGD(TAG, "Decoded 0x%02X", remote_state);
  if ((remote_state & 0xFF00000) != 0x8800000)
    return false;

  if ((remote_state & COMMAND_MASK) == COMMAND_ON) {
    this->mode = climate::CLIMATE_MODE_COOL;
  } else if ((remote_state & COMMAND_MASK) == COMMAND_ON_AI) {
    this->mode = climate::CLIMATE_MODE_AUTO;
  }

  if ((remote_state & COMMAND_MASK) == COMMAND_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if ((remote_state & COMMAND_MASK) == COMMAND_SWING) {
    this->swing_mode =
        this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  } else {
    if ((remote_state & COMMAND_MASK) == COMMAND_AUTO)
      this->mode = climate::CLIMATE_MODE_AUTO;
    else if ((remote_state & COMMAND_MASK) == COMMAND_DRY_FAN) {
      this->mode = climate::CLIMATE_MODE_DRY;
    } else {
      this->mode = climate::CLIMATE_MODE_COOL;
    }
  }

  // Temperature
  if (this->mode == climate::CLIMATE_MODE_COOL)
    this->target_temperature = ((remote_state & TEMP_MASK) >> TEMP_SHIFT) + 15;

  // Fan Speed
  if (this->mode == climate::CLIMATE_MODE_AUTO) {
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  } else if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_DRY) {
    if ((remote_state & FAN_MASK) == FAN_AUTO)
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    else if ((remote_state & FAN_MASK) == FAN_MIN)
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    else if ((remote_state & FAN_MASK) == FAN_MED)
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    else if ((remote_state & FAN_MASK) == FAN_MAX)
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
  }
  this->publish_state();

  return true;
}
void LgIrClimate::transmit_(uint32_t value) {
  calc_checksum_(value);
  ESP_LOGD(TAG, "Sending climate_lg_ir code: 0x%02X", value);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  data->reserve(2 + BITS * 2u);

  data->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (BITS - 1); mask != 0; mask >>= 1) {
    if (value & mask)
      data->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    else
      data->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  }
  data->mark(BIT_HIGH_US);
  transmit.perform();
}
void LgIrClimate::calc_checksum_(uint32_t &value) {
  uint32_t mask = 0xF;
  uint32_t sum = 0;
  for (uint8_t i = 1; i < 8; i++) {
    sum += (value & (mask << (i * 4))) >> (i * 4);
  }

  value |= (sum & mask);
}

}  // namespace climate_ir_lg
}  // namespace esphome
