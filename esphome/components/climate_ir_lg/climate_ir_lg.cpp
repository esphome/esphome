#include "climate_ir_lg.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_ir_lg {

static const char *const TAG = "climate.climate_ir_lg";

// Commands
const uint32_t COMMAND_MASK = 0xFF000;
const uint32_t COMMAND_OFF = 0xC0000;
const uint32_t COMMAND_SWING = 0x10000;

const uint32_t COMMAND_ADV_SWING = 0x13000;
const uint32_t COMMAND_ADV_SWING_DATA_MASK = 0x1F0;  // Only 5 bits are relevant

// Commands for Advanced Vertical Swing + 6 fixed positions
const uint32_t COMMAND_ADV_VERT_FIX_1 = 0x040;  // Down
const uint32_t COMMAND_ADV_VERT_FIX_2 = 0x050;
const uint32_t COMMAND_ADV_VERT_FIX_3 = 0x060;
const uint32_t COMMAND_ADV_VERT_FIX_4 = 0x070;
const uint32_t COMMAND_ADV_VERT_FIX_5 = 0x080;
const uint32_t COMMAND_ADV_VERT_FIX_6 = 0x090;      // Up
const uint32_t COMMAND_ADV_VERT_SWING_ON = 0x140;   // Swing between 1 and 6
const uint32_t COMMAND_ADV_VERT_SWING_OFF = 0x150;  // Stops immediately

// Commands for Advanced Horizontal Swing (3 modes) + 5 fixed positions
const uint32_t COMMAND_ADV_HORI_FIX_1 = 0x0B0;  // Left
const uint32_t COMMAND_ADV_HORI_FIX_2 = 0x0C0;
const uint32_t COMMAND_ADV_HORI_FIX_3 = 0x0D0;
const uint32_t COMMAND_ADV_HORI_FIX_4 = 0x0E0;
const uint32_t COMMAND_ADV_HORI_FIX_5 = 0x0F0;           // Right
const uint32_t COMMAND_ADV_HORI_SWING_ON_LEFT = 0x100;   // Swing between 1 and 3
const uint32_t COMMAND_ADV_HORI_SWING_ON_RIGHT = 0x110;  // Swing between 3 and 5
const uint32_t COMMAND_ADV_HORI_SWING_ON_FULL = 0x160;   // Swing between 1 and 5
const uint32_t COMMAND_ADV_HORI_SWING_OFF = 0x170;       // Stops immediately

const uint32_t COMMAND_ON_COOL = 0x00000;
const uint32_t COMMAND_ON_DRY = 0x01000;
const uint32_t COMMAND_ON_FAN_ONLY = 0x02000;
const uint32_t COMMAND_ON_AI = 0x03000;
const uint32_t COMMAND_ON_HEAT = 0x04000;

const uint32_t COMMAND_COOL = 0x08000;
const uint32_t COMMAND_DRY = 0x09000;
const uint32_t COMMAND_FAN_ONLY = 0x0A000;
const uint32_t COMMAND_AI = 0x0B000;
const uint32_t COMMAND_HEAT = 0x0C000;

// Fan speed
const uint32_t FAN_MASK = 0xF0;
const uint32_t FAN_AUTO = 0x50;
const uint32_t FAN_MIN = 0x00;  // AKA F1
const uint32_t FAN_F2 = 0x90;
const uint32_t FAN_MED = 0x20;  // AKA F3
const uint32_t FAN_F4 = 0xA0;
const uint32_t FAN_MAX = 0x40;  // AKA F4

// Temperature
const uint8_t TEMP_RANGE = TEMP_MAX - TEMP_MIN + 1;
const uint32_t TEMP_MASK = 0xF00;
const uint32_t TEMP_SHIFT = 8;

const uint16_t BITS = 28;

void LgIrClimate::transmit_state() {
  uint32_t remote_state = 0x8800000;

  // ESP_LOGD(TAG, "climate_lg_ir mode_before_ code: 0x%02X", modeBefore_);

  // Set command
  if (this->send_swing_cmd_) {
    this->send_swing_cmd_ = false;
    if (this->alternative_mode_) {
      switch (this->swing_mode) {
        case climate::CLIMATE_SWING_VERTICAL:
          ESP_LOGD(TAG, "setting swing vertical");
          remote_state |= COMMAND_ADV_SWING;
          remote_state |= COMMAND_ADV_VERT_SWING_ON;
          break;
        case climate::CLIMATE_SWING_OFF:
          ESP_LOGD(TAG, "setting swing off");
          remote_state |= COMMAND_ADV_SWING;
          remote_state |= COMMAND_ADV_VERT_FIX_4;
          break;
        default:
          return;  // Supress clang error, this integration only supports OFF and VERTICAL, this will never be reached
      }
      this->transmit_(remote_state);
      this->publish_state();
      return;
    } else {
      remote_state |= COMMAND_SWING;
    }
  } else {
    bool climate_is_off = (this->mode_before_ == climate::CLIMATE_MODE_OFF);
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        remote_state |= climate_is_off ? COMMAND_ON_COOL : COMMAND_COOL;
        break;
      case climate::CLIMATE_MODE_DRY:
        remote_state |= climate_is_off ? COMMAND_ON_DRY : COMMAND_DRY;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        remote_state |= climate_is_off ? COMMAND_ON_FAN_ONLY : COMMAND_FAN_ONLY;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        remote_state |= climate_is_off ? COMMAND_ON_AI : COMMAND_AI;
        break;
      case climate::CLIMATE_MODE_HEAT:
        remote_state |= climate_is_off ? COMMAND_ON_HEAT : COMMAND_HEAT;
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        remote_state |= COMMAND_OFF;
        break;
    }
  }

  this->mode_before_ = this->mode;

  ESP_LOGD(TAG, "climate_lg_ir mode code: 0x%02X", this->mode);

  // Set fan speed
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    remote_state |= FAN_AUTO;
  } else {
    switch (this->fan_mode.value()) {
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

  // Set temperature
  if ((this->mode == climate::CLIMATE_MODE_HEAT_COOL && this->alternative_mode_) ||
      this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT) {
    auto temp = (uint8_t) roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX));
    remote_state |= ((temp - 15) << TEMP_SHIFT);
  }

  this->transmit_(remote_state);
  this->publish_state();
}

bool LgIrClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t nbits = 0;
  uint32_t remote_state = 0;

  if (!data.expect_item(this->header_high_, this->header_low_))
    return false;

  for (nbits = 0; nbits < 32; nbits++) {
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      remote_state = (remote_state << 1) | 1;
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      remote_state = (remote_state << 1) | 0;
    } else if (nbits == BITS) {
      break;
    } else {
      return false;
    }
  }

  ESP_LOGD(TAG, "Decoded 0x%02" PRIX32, remote_state);
  if ((remote_state & 0xFF00000) != 0x8800000)
    return false;

  // Get command
  if ((remote_state & COMMAND_MASK) == COMMAND_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if ((remote_state & COMMAND_MASK) == COMMAND_SWING) {
    this->swing_mode =
        this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  } else if ((remote_state & COMMAND_MASK) == COMMAND_ADV_SWING) {
    ESP_LOGD(TAG, "Got advanced swing command! With data: 0x%02" PRIX32, remote_state & COMMAND_ADV_SWING_DATA_MASK);
    switch (remote_state & COMMAND_ADV_SWING_DATA_MASK) {
      case COMMAND_ADV_VERT_SWING_ON:
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        break;
      case COMMAND_ADV_HORI_SWING_ON_LEFT:
      case COMMAND_ADV_HORI_SWING_ON_RIGHT:
      case COMMAND_ADV_HORI_SWING_ON_FULL:
      case COMMAND_ADV_HORI_SWING_OFF:
      case COMMAND_ADV_HORI_FIX_1:
      case COMMAND_ADV_HORI_FIX_2:
      case COMMAND_ADV_HORI_FIX_3:
      case COMMAND_ADV_HORI_FIX_4:
      case COMMAND_ADV_HORI_FIX_5:
        // Ignore horizontal swing decoding for now.
        // this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        break;
      default:
        this->swing_mode = climate::CLIMATE_SWING_OFF;
    }

    this->publish_state();
    return true;

  } else {
    switch (remote_state & COMMAND_MASK) {
      case COMMAND_DRY:
      case COMMAND_ON_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case COMMAND_FAN_ONLY:
      case COMMAND_ON_FAN_ONLY:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case COMMAND_AI:
      case COMMAND_ON_AI:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case COMMAND_HEAT:
      case COMMAND_ON_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case COMMAND_COOL:
      case COMMAND_ON_COOL:
      default:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
    }

    // Get fan speed
    if (this->mode == climate::CLIMATE_MODE_HEAT_COOL && !(this->alternative_mode_)) {
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    } else if (this->mode == climate::CLIMATE_MODE_HEAT_COOL || this->mode == climate::CLIMATE_MODE_COOL ||
               this->mode == climate::CLIMATE_MODE_DRY || this->mode == climate::CLIMATE_MODE_FAN_ONLY ||
               this->mode == climate::CLIMATE_MODE_HEAT) {
      if ((remote_state & FAN_MASK) == FAN_AUTO) {
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
      } else if ((remote_state & FAN_MASK) == FAN_MIN) {
        this->fan_mode = climate::CLIMATE_FAN_LOW;
      } else if ((remote_state & FAN_MASK) == FAN_MED) {
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      } else if ((remote_state & FAN_MASK) == FAN_MAX) {
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
      }
    }

    // Get temperature
    if ((this->mode == climate::CLIMATE_MODE_HEAT_COOL && this->alternative_mode_) ||
        this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT) {
      this->target_temperature = ((remote_state & TEMP_MASK) >> TEMP_SHIFT) + 15;
    }
  }
  this->publish_state();

  return true;
}

void LgIrClimate::transmit_(uint32_t value) {
  this->calc_checksum_(value);
  ESP_LOGD(TAG, "Sending climate_lg_ir code: 0x%02" PRIX32, value);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  data->reserve(2 + BITS * 2u);

  data->item(this->header_high_, this->header_low_);

  for (uint32_t mask = 1UL << (BITS - 1); mask != 0; mask >>= 1) {
    if (value & mask) {
      data->item(this->bit_high_, this->bit_one_low_);
    } else {
      data->item(this->bit_high_, this->bit_zero_low_);
    }
  }
  data->mark(this->bit_high_);
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
