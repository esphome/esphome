#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

#include <algorithm>

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.climate");

static constexpr remote_base::BitTimings GREE_BIT_TIMINGS{
    .bit_mark = GREE_BIT_MARK, .one_space = GREE_ONE_SPACE, .zero_space = GREE_ZERO_SPACE};

void GreeClimate::set_model(Model model) {
  if (model == GREE_YAN || model == GREE_YX1FF) {
    // These models only expose vertical swing; horizontal swing is not encoded on the wire.
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

  this->model_ = model;
}

void GreeClimate::set_mode_bit(uint8_t bit_mask, bool enabled) {
  assign_bits(this->mode_bits_, bit_mask, enabled);
  this->transmit_state();
}

void GreeClimate::transmit_state() {
  uint8_t remote_state[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};
  const Model model = this->model_;
  // YAW1F uses the YAG byte layout with different framing (see below)
  const bool yag_layout = model == GREE_YAG || model == GREE_YAW1F;

  remote_state[0] = this->fan_speed_() | this->operation_mode_();
  remote_state[1] = this->temperature_();

  if (model == GREE_YAN) {
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[4] = this->vertical_swing_();
  }

  if (model == GREE_YX1FF || yag_layout) {
    remote_state[2] = 0x60;
    remote_state[3] = 0x50;
    remote_state[4] = this->vertical_swing_();

    if (model == GREE_YX1FF) {
      // ModelA is set only when powered on.
      if (this->mode == climate::CLIMATE_MODE_OFF)
        remote_state[2] &= ~0x40;
      // YX1FF needs the swing flag as well as its own vane-position value
      if (this->vertical_swing_() == GREE_VDIR_SWING) {
        remote_state[0] |= (1 << 6);
        remote_state[4] = GREE_YX1FF_VANE_SWING;
      }
    }
  }

  if (yag_layout) {
    remote_state[5] = model == GREE_YAW1F ? 0x80 : 0x40;

    if (this->vertical_swing_() == GREE_VDIR_SWING || this->horizontal_swing_() == GREE_HDIR_SWING) {
      remote_state[0] |= (1 << 6);
    }
  }

  if (model == GREE_YAC)
    remote_state[4] = this->vertical_swing_();

  if (model == GREE_YAC || yag_layout) {
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
    if (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO) == climate::CLIMATE_FAN_HIGH) {
      remote_state[2] |= GREE_FAN_TURBO_BIT;
    }
  }

  if (this->preset_() == GREE_PRESET_SLEEP) {
    remote_state[0] |= GREE_PRESET_SLEEP_BIT;
  }

  // Calculate the checksum
  if (model == GREE_YAN) {
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

  const bool long_gaps = model == GREE_YAC1FB9 || model == GREE_YAW1F;
  const uint32_t header_space = long_gaps ? GREE_YAC1FB9_HEADER_SPACE : GREE_HEADER_SPACE;
  const uint32_t message_space = long_gaps ? GREE_YAC1FB9_MESSAGE_SPACE : GREE_MESSAGE_SPACE;
  auto encode = [&](const uint8_t *state) {
    data->item(GREE_HEADER_MARK, header_space);
    data->item_bytes<GREE_BIT_TIMINGS>(state, 4);
    // Fixed 010 between the two halves, then the message gap
    data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);
    data->item(GREE_BIT_MARK, GREE_ONE_SPACE);
    data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);
    data->item(GREE_BIT_MARK, message_space);
    data->item_bytes<GREE_BIT_TIMINGS>(state + 4, 4);
    data->mark(GREE_BIT_MARK);
  };

  encode(remote_state);
#ifdef USE_GREE_YAW1F
  if (model == GREE_YAW1F) {
    // The YAW1F remote follows every command with a second message, 00 00 00 A0 | 00 00 00 A0.
    // The command buffer is reused so the payload takes no RAM on ESP8266.
    std::fill_n(remote_state, sizeof(remote_state), 0);
    remote_state[3] = remote_state[7] = 0xA0;
    data->space(GREE_YAW1F_FOLLOWUP_SPACE);
    encode(remote_state);
  }
#endif
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
      case climate::CLIMATE_FAN_HIGH:
        // HIGH additionally sets the separate Turbo bit in byte 2.
        return GREE_FAN_3;
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
