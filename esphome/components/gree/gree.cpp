#include "gree.h"
#include <cstring>
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.climate");

static void gree_yap_send_byte(remote_base::RemoteTransmitData *data, uint8_t value) {
  for (uint8_t mask = 1; mask > 0; mask <<= 1) {
    data->mark(GREE_YAP_BIT_MARK);
    data->space((value & mask) ? GREE_YAP_ONE_SPACE : GREE_YAP_ZERO_SPACE);
  }
}

// YAP1F checksum (HeatpumpIR GreeYAP): nibble sum over bytes 0-3 and high nibbles of 5-7.
static uint8_t gree_yap_checksum(const uint8_t *buffer) {
  const uint8_t sum = (buffer[0] & 0x0F) + (buffer[1] & 0x0F) + (buffer[2] & 0x0F) + (buffer[3] & 0x0F) +
                      ((buffer[5] & 0xF0) >> 4) + ((buffer[6] & 0xF0) >> 4) + ((buffer[7] & 0xF0) >> 4) + 0x0A;
  return uint8_t(((sum & 0x0F) << 4) | (buffer[7] & 0x0F));
}

void GreeClimate::set_model(Model model) {
  if (model == GREE_YAN) {
    // YAN only has a vertical vane; the horizontal swing IR bytes are not defined for this model.
    this->swing_modes_.erase(climate::CLIMATE_SWING_HORIZONTAL);
    this->swing_modes_.erase(climate::CLIMATE_SWING_BOTH);
  }
  if (model == GREE_YX1FF) {
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);  // YX1FF 4 speed
  }
  // Models with a sleep preset
  if (model == GREE_YX1FF || model == GREE_YAC1FB9) {
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);
  }
  // YAP1F has no swing IR control in the current frame layout.
  if (model == GREE_YAP1F) {
    this->swing_modes_ = {climate::CLIMATE_SWING_OFF};
  }

  this->model_ = model;
}

void GreeClimate::set_mode_bit(uint8_t bit_mask, bool enabled) {
  assign_bits(this->mode_bits_, bit_mask, enabled);
  this->transmit_state();
}

void GreeClimate::transmit_state() {
  if (this->model_ == GREE_YAP1F) {
    uint8_t buffer[25] = {0};

    // Match HeatpumpIR's parameter conversion defaults/quirks for this protocol.
    const bool power_off = this->mode == climate::CLIMATE_MODE_OFF;

    uint8_t operating_mode = GREE_MODE_HEAT;
    uint8_t temperature_cmd = uint8_t(roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX)));
    uint8_t fan_speed = this->fan_speed_();

    if (!power_off) {
      switch (this->mode) {
        case climate::CLIMATE_MODE_HEAT_COOL:
        case climate::CLIMATE_MODE_AUTO:
          operating_mode = GREE_MODE_AUTO;
          temperature_cmd = 25;
          break;
        case climate::CLIMATE_MODE_COOL:
          operating_mode = GREE_MODE_COOL;
          break;
        case climate::CLIMATE_MODE_DRY:
          operating_mode = GREE_MODE_DRY;
          fan_speed = GREE_FAN_1;
          break;
        case climate::CLIMATE_MODE_FAN_ONLY:
          operating_mode = GREE_MODE_FAN;
          break;
        case climate::CLIMATE_MODE_HEAT:
        default:
          operating_mode = GREE_MODE_HEAT;
          break;
      }
    } else {
      // Match HeatpumpIR's defaults when sending POWER_OFF: fan=auto, mode=heat.
      fan_speed = GREE_FAN_AUTO;
    }

    buffer[0] = fan_speed | operating_mode | (power_off ? 0 : GREE_MODE_ON);
    buffer[1] = uint8_t(clamp<int>(temperature_cmd, GREE_TEMP_MIN, GREE_TEMP_MAX) - GREE_TEMP_MIN);
    buffer[2] = this->mode_bits_;
    buffer[3] = 0x50;
    // The YAP frame sends a fixed 010 marker in place of buffer[4].
    // Keep vane defaults disabled until their serialized location is known.
    buffer[4] = 0x00;
    buffer[5] = 0xC2;  // 0x82 + WiFi enabled (bit 6)

    // Copy blocks (mirrors HeatpumpIR's GreeYAP implementation)
    memcpy(buffer + 8, buffer, 3);
    buffer[11] = 0x70;
    buffer[19] = 0xA0;
    buffer[23] = 0xA0;
    buffer[24] = 0x00;

    buffer[8] = gree_yap_checksum(buffer);
    buffer[16] = gree_yap_checksum(buffer + 8);

    auto transmit = this->transmitter_->transmit();
    auto *data = transmit.get_data();
    data->set_carrier_frequency(GREE_YAP_IR_FREQUENCY);

    for (size_t pos = 0; pos < 24; pos += 8) {
      data->mark(GREE_YAP_HEADER_MARK);
      data->space(GREE_YAP_HEADER_SPACE);

      for (size_t i = 0; i < 4; i++) {
        gree_yap_send_byte(data, buffer[pos + i]);
      }

      // Fixed bits: 010
      data->mark(GREE_YAP_BIT_MARK);
      data->space(GREE_YAP_ZERO_SPACE);
      data->mark(GREE_YAP_BIT_MARK);
      data->space(GREE_YAP_ONE_SPACE);
      data->mark(GREE_YAP_BIT_MARK);
      data->space(GREE_YAP_ZERO_SPACE);

      data->mark(GREE_YAP_BIT_MARK);
      data->space(GREE_YAP_MESSAGE_SPACE);

      for (size_t i = 5; i < 9; i++) {
        gree_yap_send_byte(data, buffer[pos + i]);
      }

      data->mark(GREE_YAP_BIT_MARK);
      data->space(pos + 8 < 24 ? GREE_YAP_MESSAGE_SPACE : 0);
    }

    transmit.perform();
    return;
  }

  uint8_t remote_state[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};
  const Model model = this->model_;

  remote_state[0] = this->fan_speed_() | this->operation_mode_();
  remote_state[1] = this->temperature_();

  if (model == GREE_YAN) {
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[4] = this->vertical_swing_();
  }

  if (model == GREE_YX1FF || model == GREE_YAG) {
    remote_state[2] = 0x60;
    remote_state[3] = 0x50;
    remote_state[4] = this->vertical_swing_();
  }

  if (model == GREE_YAG) {
    remote_state[5] = 0x40;

    if (this->vertical_swing_() == GREE_VDIR_SWING || this->horizontal_swing_() == GREE_HDIR_SWING) {
      remote_state[0] |= (1 << 6);
    }
  }

  if (model == GREE_YAC || model == GREE_YAG) {
    remote_state[4] |= (this->horizontal_swing_() << 4);
  }

  if (model == GREE_YAA || model == GREE_YAC || model == GREE_YAC1FB9) {
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[6] = 0x20;  // YAA1FB, FAA1FB1, YB1F2 bits 4..7 always 0010

    if (this->vertical_swing_() == GREE_VDIR_SWING) {
      remote_state[0] |= (1 << 6);  // Enable swing by setting bit 6
    } else if (this->vertical_swing_() != GREE_VDIR_AUTO) {
      remote_state[5] = this->vertical_swing_();
    }
  }

  if (model == GREE_YAN || model == GREE_YAA || model == GREE_YAC || model == GREE_YAC1FB9) {
    // Merge the mode bits into remote_state[2]
    // Clear the mode bits (bits 4-7) and OR in the current mode_bits_
    remote_state[2] = (remote_state[2] & 0x0F) | this->mode_bits_;
  }

  if (model == GREE_YX1FF) {
    if (this->fan_speed_() == GREE_FAN_TURBO) {
      remote_state[2] |= GREE_FAN_TURBO_BIT;
    }
  }

  if (this->preset_() == GREE_PRESET_SLEEP) {
    remote_state[0] |= GREE_PRESET_SLEEP_BIT;
  }

  // Calculate the checksum
  if (model == GREE_YAN || model == GREE_YX1FF) {
    remote_state[7] = ((remote_state[0] << 4) + (remote_state[1] << 4) + 0xC0);
  } else {
    remote_state[7] =
        ((((remote_state[0] & 0x0F) + (remote_state[1] & 0x0F) + (remote_state[2] & 0x0F) + (remote_state[3] & 0x0F) +
           ((remote_state[4] & 0xF0) >> 4) + ((remote_state[5] & 0xF0) >> 4) + ((remote_state[6] & 0xF0) >> 4) + 0x0A) &
          0x0F)
         << 4);
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(GREE_IR_FREQUENCY);

  data->mark(GREE_HEADER_MARK);
  if (model == GREE_YAC1FB9) {
    data->space(GREE_YAC1FB9_HEADER_SPACE);
  } else {
    data->space(GREE_HEADER_SPACE);
  }

  for (int i = 0; i < 4; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(GREE_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->mark(GREE_BIT_MARK);
  data->space(GREE_ZERO_SPACE);
  data->mark(GREE_BIT_MARK);
  data->space(GREE_ONE_SPACE);
  data->mark(GREE_BIT_MARK);
  data->space(GREE_ZERO_SPACE);

  data->mark(GREE_BIT_MARK);
  if (model == GREE_YAC1FB9) {
    data->space(GREE_YAC1FB9_MESSAGE_SPACE);
  } else {
    data->space(GREE_MESSAGE_SPACE);
  }

  for (int i = 4; i < 8; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(GREE_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->mark(GREE_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t GreeClimate::operation_mode_() {
  uint8_t operating_mode = GREE_MODE_ON;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= GREE_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= GREE_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= GREE_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= GREE_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= GREE_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = GREE_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint8_t GreeClimate::fan_speed_() {
  // YX1FF has 4 fan speeds -- we treat low as quiet and turbo as high
  if (this->model_ == GREE_YX1FF) {
    switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
      case climate::CLIMATE_FAN_QUIET:
        return GREE_FAN_1;
      case climate::CLIMATE_FAN_LOW:
        return GREE_FAN_2;
      case climate::CLIMATE_FAN_MEDIUM:
        return GREE_FAN_3;
      case climate::CLIMATE_FAN_HIGH:
        return GREE_FAN_TURBO;
      case climate::CLIMATE_FAN_AUTO:
      default:
        return GREE_FAN_AUTO;
    }
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
    case climate::CLIMATE_FAN_LOW:
      return GREE_FAN_1;
    case climate::CLIMATE_FAN_MEDIUM:
      return GREE_FAN_2;
    case climate::CLIMATE_FAN_HIGH:
      return GREE_FAN_3;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return GREE_FAN_AUTO;
  }
}

uint8_t GreeClimate::horizontal_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_HDIR_SWING;
    default:
      return GREE_HDIR_MANUAL;
  }
}

uint8_t GreeClimate::vertical_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_VDIR_SWING;
    default:
      return GREE_VDIR_MANUAL;
  }
}

uint8_t GreeClimate::temperature_() {
  return (uint8_t) roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX)) - GREE_TEMP_MIN;
}

uint8_t GreeClimate::preset_() {
  // YX1FF and YAC1FB9 have sleep preset
  if (this->model_ == GREE_YX1FF || this->model_ == GREE_YAC1FB9) {
    switch (this->preset.value_or(climate::CLIMATE_PRESET_NONE)) {
      case climate::CLIMATE_PRESET_NONE:
        return GREE_PRESET_NONE;
      case climate::CLIMATE_PRESET_SLEEP:
        return GREE_PRESET_SLEEP;
      default:
        return GREE_PRESET_NONE;
    }
  }

  return GREE_PRESET_NONE;
}

}  // namespace esphome::gree
