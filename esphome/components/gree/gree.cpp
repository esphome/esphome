#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.climate");

static uint8_t gree_standard_checksum(const uint8_t *buffer) {
  const uint8_t sum = (buffer[0] & 0x0F) + (buffer[1] & 0x0F) + (buffer[2] & 0x0F) + (buffer[3] & 0x0F) +
                      ((buffer[4] & 0xF0) >> 4) + ((buffer[5] & 0xF0) >> 4) + ((buffer[6] & 0xF0) >> 4) + 0x0A;
  return uint8_t(((sum & 0x0F) << 4) | (buffer[7] & 0x0F));
}

static uint8_t gree_yan_checksum(const uint8_t *buffer) { return uint8_t((buffer[0] << 4) + (buffer[1] << 4) + 0xC0); }

static uint8_t gree_yag_checksum(const uint8_t *buffer) {
  const uint8_t sum = (buffer[0] & 0x0F) + (buffer[1] & 0x0F) + (buffer[2] & 0x0F) + (buffer[3] & 0x0F) +
                      ((buffer[4] & 0xF0) >> 4) + ((buffer[5] & 0xF0) >> 4) + ((buffer[6] & 0xF0) >> 4) + 0x0A;
  return uint8_t((sum & 0x0F) << 4);
}

static bool gree_is_fixed_vertical_direction(uint8_t direction) {
  switch (direction) {
    case GREE_VDIR_AUTO:
    case GREE_VDIR_UP:
    case GREE_VDIR_MUP:
    case GREE_VDIR_MIDDLE:
    case GREE_VDIR_MDOWN:
    case GREE_VDIR_DOWN:
      return true;
    default:
      return false;
  }
}

static bool gree_is_fixed_horizontal_direction(uint8_t direction) {
  switch (direction) {
    case GREE_HDIR_AUTO:
    case GREE_HDIR_LEFT:
    case GREE_HDIR_MLEFT:
    case GREE_HDIR_MIDDLE:
    case GREE_HDIR_MRIGHT:
    case GREE_HDIR_RIGHT:
      return true;
    default:
      return false;
  }
}

static uint32_t gree_header_space(Model model) {
  if (model == GREE_YAC1FB9) {
    return GREE_YAC1FB9_HEADER_SPACE;
  }
  return GREE_HEADER_SPACE;
}

static uint32_t gree_message_space(Model model) {
  if (model == GREE_YAC1FB9) {
    return GREE_YAC1FB9_MESSAGE_SPACE;
  }
  return GREE_MESSAGE_SPACE;
}

static climate::ClimateMode gree_decode_mode(uint8_t mode) {
  switch (mode) {
    case GREE_MODE_AUTO:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case GREE_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case GREE_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case GREE_MODE_FAN:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case GREE_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

static climate::ClimateFanMode gree_decode_fan(uint8_t fan, bool yx1ff) {
  if (yx1ff) {
    switch (fan) {
      case GREE_FAN_1:
        return climate::CLIMATE_FAN_QUIET;
      case GREE_FAN_2:
        return climate::CLIMATE_FAN_LOW;
      case GREE_FAN_3:
        return climate::CLIMATE_FAN_MEDIUM;
      case GREE_FAN_TURBO:
        return climate::CLIMATE_FAN_HIGH;
      case GREE_FAN_AUTO:
      default:
        return climate::CLIMATE_FAN_AUTO;
    }
  }

  switch (fan) {
    case GREE_FAN_1:
      return climate::CLIMATE_FAN_LOW;
    case GREE_FAN_2:
      return climate::CLIMATE_FAN_MEDIUM;
    case GREE_FAN_3:
      return climate::CLIMATE_FAN_HIGH;
    case GREE_FAN_AUTO:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

// Reads one byte sent least significant bit first.
static bool gree_read_byte(remote_base::RemoteReceiveData &data, uint8_t *value) {
  uint8_t out = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
      out |= static_cast<uint8_t>(1U << bit);
    } else if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
      return false;
    }
  }
  *value = out;
  return true;
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

  this->model_ = model;
}

void GreeClimate::set_mode_bit(uint8_t bit_mask, bool enabled) {
  assign_bits(this->mode_bits_, bit_mask, enabled);
  this->transmit_state();
}

// Received vane positions are validated but not stored. Writing them to the configured defaults would
// overwrite the user's default_vertical_direction / default_horizontal_direction on every received frame.
bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t buffer[GREE_STATE_FRAME_SIZE] = {0};

  if (!data.expect_item(GREE_HEADER_MARK, gree_header_space(this->model_))) {
    return false;
  }

  for (size_t i = 0; i < 4; i++) {
    if (!gree_read_byte(data, &buffer[i])) {
      return false;
    }
  }

  if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE) ||
      !data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
    return false;
  }

  if (!data.expect_item(GREE_BIT_MARK, gree_message_space(this->model_))) {
    return false;
  }

  for (size_t i = 4; i < 8; i++) {
    if (!gree_read_byte(data, &buffer[i])) {
      return false;
    }
  }

  if (!data.expect_mark(GREE_BIT_MARK)) {
    return false;
  }

  uint8_t expected_checksum = 0;
  switch (this->model_) {
    case GREE_YAN:
    case GREE_YX1FF:
      expected_checksum = gree_yan_checksum(buffer);
      break;
    case GREE_YAG:
      expected_checksum = gree_yag_checksum(buffer);
      break;
    default:
      expected_checksum = gree_standard_checksum(buffer);
      break;
  }

  if (buffer[7] != expected_checksum) {
    ESP_LOGVV(TAG, "Checksum mismatch (got %02X expected %02X)", buffer[7], expected_checksum);
    return false;
  }

  // Validate every field before writing any state, so a rejected frame changes nothing.
  if (buffer[1] > (GREE_TEMP_MAX - GREE_TEMP_MIN)) {
    ESP_LOGVV(TAG, "Unsupported temperature value: 0x%02X", buffer[1]);
    return false;
  }

  bool vertical_swing = false;
  bool horizontal_swing = false;
  bool supports_horizontal_swing = false;

  if (this->model_ == GREE_YAN || this->model_ == GREE_YX1FF) {
    const uint8_t vdir = buffer[4] & 0x0F;
    vertical_swing = vdir == GREE_VDIR_SWING;
    if (!vertical_swing && !gree_is_fixed_vertical_direction(vdir)) {
      ESP_LOGVV(TAG, "Unsupported vertical direction value: 0x%02X", vdir);
      return false;
    }
  } else if (this->model_ == GREE_YAA || this->model_ == GREE_YAC || this->model_ == GREE_YAC1FB9) {
    vertical_swing = (buffer[0] & (1 << 6)) != 0;
    if (!vertical_swing) {
      const uint8_t vdir = buffer[5] & 0x0F;
      if (!gree_is_fixed_vertical_direction(vdir)) {
        ESP_LOGVV(TAG, "Unsupported vertical direction value: 0x%02X", vdir);
        return false;
      }
    }
    if (this->model_ == GREE_YAC) {
      supports_horizontal_swing = true;
      const uint8_t hdir = (buffer[4] >> 4) & 0x0F;
      horizontal_swing = hdir == GREE_HDIR_SWING;
      if (!horizontal_swing && !gree_is_fixed_horizontal_direction(hdir)) {
        ESP_LOGVV(TAG, "Unsupported horizontal direction value: 0x%02X", hdir);
        return false;
      }
    }
  } else if (this->model_ == GREE_YAG) {
    supports_horizontal_swing = true;
    const uint8_t vdir = buffer[4] & 0x0F;
    const uint8_t hdir = (buffer[4] >> 4) & 0x0F;
    vertical_swing = vdir == GREE_VDIR_SWING;
    horizontal_swing = hdir == GREE_HDIR_SWING;
    if (!vertical_swing && !gree_is_fixed_vertical_direction(vdir)) {
      ESP_LOGVV(TAG, "Unsupported vertical direction value: 0x%02X", vdir);
      return false;
    }
    if (!horizontal_swing && !gree_is_fixed_horizontal_direction(hdir)) {
      ESP_LOGVV(TAG, "Unsupported horizontal direction value: 0x%02X", hdir);
      return false;
    }
  }

  const bool power_on = (buffer[0] & GREE_MODE_ON) != 0;
  this->mode = power_on ? gree_decode_mode(buffer[0] & 0x07) : climate::CLIMATE_MODE_OFF;
  this->target_temperature = float(buffer[1] + GREE_TEMP_MIN);

  if (this->model_ == GREE_YX1FF) {
    // Turbo also sets byte 0 bit 7, so sleep is only read when turbo is off.
    const bool turbo = (buffer[2] & GREE_FAN_TURBO_BIT) != 0;
    this->fan_mode = gree_decode_fan(turbo ? GREE_FAN_TURBO : (buffer[0] & 0x30), true);
    this->preset =
        !turbo && (buffer[0] & GREE_PRESET_SLEEP_BIT) ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;
  } else {
    this->fan_mode = gree_decode_fan(buffer[0] & 0x30, false);
    if (this->model_ == GREE_YAC1FB9) {
      this->preset = (buffer[0] & GREE_PRESET_SLEEP_BIT) ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;
    }
  }

  // Generic frames carry no vane state, so the swing mode is left as it is.
  if (this->model_ != GREE_GENERIC) {
    climate::ClimateSwingMode swing_mode;
    if (supports_horizontal_swing) {
      if (vertical_swing && horizontal_swing) {
        swing_mode = climate::CLIMATE_SWING_BOTH;
      } else if (vertical_swing) {
        swing_mode = climate::CLIMATE_SWING_VERTICAL;
      } else if (horizontal_swing) {
        swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      } else {
        swing_mode = climate::CLIMATE_SWING_OFF;
      }
    } else {
      swing_mode = vertical_swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
    }
    this->swing_mode = swing_mode;
  }

  // Only these models merge mode bits into the frame in transmit_state(), so only they carry them.
  if (this->model_ == GREE_YAN || this->model_ == GREE_YAA || this->model_ == GREE_YAC ||
      this->model_ == GREE_YAC1FB9) {
    this->mode_bits_ = buffer[2] & 0xF0;
  }

  this->publish_state();
  return true;
}

void GreeClimate::transmit_state() {
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
