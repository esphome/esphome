#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace gree {

static const char *const TAG = "gree.climate";

void GreeClimate::set_model(Model model) {
  if (model == GREE_YX1FF) {
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);   // YX1FF 4 speed
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);   // YX1FF sleep mode
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);  // YX1FF sleep mode
  }

  this->model_ = model;
}

void GreeClimate::transmit_state() {
  uint8_t remote_state[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};

  remote_state[0] = this->fan_speed_() | this->operation_mode_();
  remote_state[1] = this->temperature_();

  if (this->model_ == GREE_YAN || this->model_ == GREE_YX1FF || this->model_ == GREE_YAG) {
    remote_state[2] = 0x60;
    remote_state[3] = 0x50;
    remote_state[4] = this->vertical_swing_();
  }

  if (this->model_ == GREE_YAG) {
    remote_state[5] = 0x40;

    if (this->vertical_swing_() == GREE_VDIR_SWING || this->horizontal_swing_() == GREE_HDIR_SWING) {
      remote_state[0] |= (1 << 6);
    }
  }

  if (this->model_ == GREE_YAC || this->model_ == GREE_YAG) {
    remote_state[4] |= (this->horizontal_swing_() << 4);
  }

  if (this->model_ == GREE_YAA || this->model_ == GREE_YAC || this->model_ == GREE_YAC1FB9) {
    remote_state[2] = 0x20;  // bits 0..3 always 0000, bits 4..7 TURBO,LIGHT,HEALTH,X-FAN
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[6] = 0x20;  // YAA1FB, FAA1FB1, YB1F2 bits 4..7 always 0010

    if (this->vertical_swing_() == GREE_VDIR_SWING) {
      remote_state[0] |= (1 << 6);  // Enable swing by setting bit 6
    } else if (this->vertical_swing_() != GREE_VDIR_AUTO) {
      remote_state[5] = this->vertical_swing_();
    }
  }

  if (this->model_ == GREE_YX1FF) {
    if (this->fan_speed_() == GREE_FAN_TURBO) {
      remote_state[2] |= GREE_FAN_TURBO_BIT;
    }

    if (this->preset_() == GREE_PRESET_SLEEP) {
      remote_state[0] |= GREE_PRESET_SLEEP_BIT;
    }
  }

  // Calculate the checksum
  if (this->model_ == GREE_YAN || this->model_ == GREE_YX1FF) {
    remote_state[7] = ((remote_state[0] << 4) + (remote_state[1] << 4) + 0xC0);
  } else if (this->model_ == GREE_YAG) {
    remote_state[7] =
        ((((remote_state[0] & 0x0F) + (remote_state[1] & 0x0F) + (remote_state[2] & 0x0F) + (remote_state[3] & 0x0F) +
           ((remote_state[4] & 0xF0) >> 4) + ((remote_state[5] & 0xF0) >> 4) + ((remote_state[6] & 0xF0) >> 4) + 0x0A) &
          0x0F)
         << 4);
  } else {
    remote_state[7] =
        ((((remote_state[0] & 0x0F) + (remote_state[1] & 0x0F) + (remote_state[2] & 0x0F) + (remote_state[3] & 0x0F) +
           ((remote_state[5] & 0xF0) >> 4) + ((remote_state[6] & 0xF0) >> 4) + ((remote_state[7] & 0xF0) >> 4) + 0x0A) &
          0x0F)
         << 4) |
        (remote_state[7] & 0x0F);
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(GREE_IR_FREQUENCY);

  data->mark(GREE_HEADER_MARK);
  if (this->model_ == GREE_YAC1FB9) {
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
  if (this->model_ == GREE_YAC1FB9) {
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
    switch (this->fan_mode.value()) {
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

  switch (this->fan_mode.value()) {
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
  return (uint8_t) roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX));
}

uint8_t GreeClimate::preset_() {
  // YX1FF has sleep preset
  if (this->model_ == GREE_YX1FF) {
    switch (this->preset.value()) {
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

}  // namespace gree
}  // namespace esphome
