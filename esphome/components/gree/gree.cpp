#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree {

static const char *const TAG = "gree.climate";

climate::ClimateTraits GreeClimate::traits() {
  auto t = climate_ir::ClimateIR::traits();
  // ClimateIR unconditionally includes HEAT_COOL in the base mode set; remove it when heat is not supported.
  if (!this->supports_heat_) {
    auto modes = t.get_supported_modes();
    modes.erase(climate::CLIMATE_MODE_HEAT_COOL);
    t.set_supported_modes(modes);
  }
  return t;
}

void GreeClimate::set_model(Model model) {
  if (model == GREE_YAN) {
    // YAN only has a vertical vane; the horizontal swing IR bytes are not defined for this model.
    this->swing_modes_.erase(climate::CLIMATE_SWING_HORIZONTAL);
    this->swing_modes_.erase(climate::CLIMATE_SWING_BOTH);
  }
  if (model == GREE_YX1FF) {
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);   // YX1FF 4 speed
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);   // YX1FF sleep mode
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);  // YX1FF sleep mode
  }

  this->model_ = model;
}

void GreeClimate::set_mode_bit(uint8_t bit_mask, bool enabled) {
  if (enabled) {
    this->mode_bits_ |= bit_mask;
  } else {
    this->mode_bits_ &= ~bit_mask;
  }
  this->transmit_state();
}

void GreeClimate::transmit_state() {
  uint8_t remote_state[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};

  remote_state[0] = this->fan_speed_() | this->operation_mode_();
  remote_state[1] = this->temperature_();

  if (this->model_ == GREE_YAN) {
    remote_state[2] = 0x20;  // bits 0..3 always 0000, bits 4..7 TURBO, LIGHT, HEALTH, X-FAN
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[4] = this->vertical_swing_();
  }

  if (this->model_ == GREE_YX1FF || this->model_ == GREE_YAG) {
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
    remote_state[2] = 0x20;  // bits 0..3 always 0000, bits 4..7 TURBO, LIGHT, HEALTH, X-FAN
    remote_state[3] = 0x50;  // bits 4..7 always 0101
    remote_state[6] = 0x20;  // YAA1FB, FAA1FB1, YB1F2 bits 4..7 always 0010

    if (this->vertical_swing_() == GREE_VDIR_SWING) {
      remote_state[0] |= (1 << 6);  // Enable swing by setting bit 6
    } else if (this->vertical_swing_() != GREE_VDIR_AUTO) {
      remote_state[5] = this->vertical_swing_();
    }
  }

  if (this->model_ == GREE_YAN || this->model_ == GREE_YAA || this->model_ == GREE_YAC ||
      this->model_ == GREE_YAC1FB9) {
    // Merge the mode bits into remote_state[2]
    // Clear the mode bits (bits 4-7) and OR in the current mode_bits_
    remote_state[2] = (remote_state[2] & 0x0F) | this->mode_bits_;
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

bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t remote_state[GREE_STATE_FRAME_SIZE] = {0};

  // Header mark and space. YAC1FB9 remotes use a slightly longer header space.
  if (!data.expect_item(GREE_HEADER_MARK, GREE_HEADER_SPACE) &&
      !data.expect_item(GREE_HEADER_MARK, GREE_YAC1FB9_HEADER_SPACE)) {
    return false;
  }

  // First half of the frame: bytes 0..3, least significant bit first.
  for (int i = 0; i < 4; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      if (data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
        remote_state[i] |= mask;
      } else if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
        return false;
      }
    }
  }

  // The two halves are joined by three separator bits (0, 1, 0).
  if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE) ||
      !data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
    return false;
  }

  // Gap between the two halves. YAC1FB9 remotes use a slightly longer gap.
  if (!data.expect_item(GREE_BIT_MARK, GREE_MESSAGE_SPACE) &&
      !data.expect_item(GREE_BIT_MARK, GREE_YAC1FB9_MESSAGE_SPACE)) {
    return false;
  }

  // Second half of the frame: bytes 4..7, least significant bit first.
  for (int i = 4; i < 8; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      if (data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
        remote_state[i] |= mask;
      } else if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
        return false;
      }
    }
  }

  if (!data.expect_mark(GREE_BIT_MARK)) {
    return false;
  }

  // Check the checksum, matching the formula transmit_state() uses for this model.
  uint8_t checksum;
  if (this->model_ == GREE_YAN || this->model_ == GREE_YX1FF) {
    checksum = (uint8_t) ((remote_state[0] << 4) + (remote_state[1] << 4) + 0xC0);
  } else {
    checksum = (uint8_t) (((remote_state[0] & 0x0F) + (remote_state[1] & 0x0F) + (remote_state[2] & 0x0F) +
                           (remote_state[3] & 0x0F) + ((remote_state[4] & 0xF0) >> 4) +
                           ((remote_state[5] & 0xF0) >> 4) + ((remote_state[6] & 0xF0) >> 4) + 0x0A)
                          << 4);
  }
  if (checksum != remote_state[7]) {
    return false;
  }

  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X   %02X %02X %02X %02X", remote_state[0], remote_state[1],
           remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6], remote_state[7]);

  // Bit 3 of byte 0 is the power state (GREE_MODE_ON); bits 0..2 hold the mode.
  if (!(remote_state[0] & GREE_MODE_ON)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (remote_state[0] & 0x07) {
      case GREE_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case GREE_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case GREE_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case GREE_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case GREE_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      default:
        return false;
    }
  }

  // Bits 4..5 of byte 0 hold the fan speed.
  switch (remote_state[0] & 0x30) {
    case GREE_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case GREE_FAN_1:
      this->fan_mode = this->model_ == GREE_YX1FF ? climate::CLIMATE_FAN_QUIET : climate::CLIMATE_FAN_LOW;
      break;
    case GREE_FAN_2:
      this->fan_mode = this->model_ == GREE_YX1FF ? climate::CLIMATE_FAN_LOW : climate::CLIMATE_FAN_MEDIUM;
      break;
    case GREE_FAN_3:
      this->fan_mode = this->model_ == GREE_YX1FF ? climate::CLIMATE_FAN_MEDIUM : climate::CLIMATE_FAN_HIGH;
      break;
  }

  // YX1FF encodes its highest fan speed (turbo) as a bit in byte 2.
  if (this->model_ == GREE_YX1FF && (remote_state[2] & GREE_FAN_TURBO_BIT)) {
    this->fan_mode = climate::CLIMATE_FAN_HIGH;
  }

  // Byte 1 holds the target temperature. Some remotes store it as an absolute
  // value (16..30), others relative to the minimum (0..14).
  if (remote_state[1] >= GREE_TEMP_MIN && remote_state[1] <= GREE_TEMP_MAX) {
    this->target_temperature = remote_state[1];
  } else if (remote_state[1] <= GREE_TEMP_MAX - GREE_TEMP_MIN) {
    this->target_temperature = remote_state[1] + GREE_TEMP_MIN;
  } else {
    return false;
  }

  // Bit 6 of byte 0 enables vertical swing; the high nibble of byte 4 holds
  // the horizontal swing position (only transmitted on YAC/YAG). On YAG bit 6
  // is a general "swing active" flag, so a horizontal-only YAG swing reads
  // back as swinging in both directions.
  bool swing_vertical = remote_state[0] & 0x40;
  bool swing_horizontal = (remote_state[4] & 0xF0) == (GREE_HDIR_SWING << 4);
  if (swing_vertical && swing_horizontal) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (swing_vertical) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_horizontal) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  // The sleep preset is only available on YX1FF. Turbo and sleep share bit 7
  // of byte 0, but turbo is additionally flagged in byte 2, so only report
  // sleep when turbo is not set.
  if (this->model_ == GREE_YX1FF) {
    bool turbo = remote_state[2] & GREE_FAN_TURBO_BIT;
    this->preset = (!turbo && (remote_state[0] & GREE_PRESET_SLEEP_BIT)) ? climate::CLIMATE_PRESET_SLEEP
                                                                         : climate::CLIMATE_PRESET_NONE;
  }

  // Remember the extra switch bits (turbo, light, health, x-fan) so the next
  // transmission reflects the state the unit acknowledged.
  if (this->model_ == GREE_YAN || this->model_ == GREE_YAA || this->model_ == GREE_YAC ||
      this->model_ == GREE_YAC1FB9) {
    this->mode_bits_ = remote_state[2] & 0xF0;
  }

  this->publish_state();
  return true;
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
  return (uint8_t) roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX));
}

uint8_t GreeClimate::preset_() {
  // YX1FF has sleep preset
  if (this->model_ == GREE_YX1FF) {
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
