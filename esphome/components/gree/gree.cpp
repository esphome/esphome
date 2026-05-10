#include "gree.h"
#include <cstring>
#include "esphome/components/remote_base/remote_base.h"
#ifdef ESPHOME_GREE_IR_DECODE_DEBUG
#include "esphome/core/log.h"
#endif

namespace esphome::gree {

#ifdef ESPHOME_GREE_IR_DECODE_DEBUG
static const char *const TAG = "gree.climate";
#define GREE_LOG_DECODE(...) ESP_LOGVV(TAG, __VA_ARGS__)
#else
#define GREE_LOG_DECODE(...)
#endif

static constexpr uint32_t GREE_YAP_IR_FREQUENCY = 38000;
static constexpr uint32_t GREE_YAP_HEADER_MARK = 9000;
static constexpr uint32_t GREE_YAP_HEADER_SPACE = 4500;
static constexpr uint32_t GREE_YAP_BIT_MARK = 650;
static constexpr uint32_t GREE_YAP_ONE_SPACE = 1643;
static constexpr uint32_t GREE_YAP_ZERO_SPACE = 510;
static constexpr uint32_t GREE_YAP_MESSAGE_SPACE = 20000;

static void gree_yap_send_byte(remote_base::RemoteTransmitData *data, uint8_t value) {
  for (uint8_t mask = 1; mask > 0; mask <<= 1) {
    data->mark(GREE_YAP_BIT_MARK);
    data->space((value & mask) ? GREE_YAP_ONE_SPACE : GREE_YAP_ZERO_SPACE);
  }
}

static uint8_t gree_yap_checksum(const uint8_t *buffer) {
  const uint8_t sum = (buffer[0] & 0x0F) + (buffer[1] & 0x0F) + (buffer[2] & 0x0F) + (buffer[3] & 0x0F) +
                      ((buffer[5] & 0xF0) >> 4) + ((buffer[6] & 0xF0) >> 4) + ((buffer[7] & 0xF0) >> 4) + 0x0A;
  return uint8_t(((sum & 0x0F) << 4) | (buffer[7] & 0x0F));
}

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

static bool gree_has_mode_bits(Model model) {
  switch (model) {
    case GREE_GENERIC:
    case GREE_YAN:
    case GREE_YAA:
    case GREE_YAC:
    case GREE_YAC1FB9:
    case GREE_YX1FF:
    case GREE_YAG:
    case GREE_YAP1F:
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

static bool gree_read_byte(remote_base::RemoteReceiveData &data, uint32_t bit_mark, uint32_t one_space,
                           uint32_t zero_space, uint8_t *value) {
  uint8_t out = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (data.expect_item(bit_mark, one_space)) {
      out |= 1 << bit;
    } else if (!data.expect_item(bit_mark, zero_space)) {
      return false;
    }
  }
  *value = out;
  return true;
}

void GreeClimate::set_model(Model model) {
  if (model == GREE_YX1FF) {
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);   // YX1FF 4 speed
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);   // YX1FF sleep mode
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);  // YX1FF sleep mode
  }

  switch (model) {
    case GREE_YAN:
    case GREE_YAA:
    case GREE_YAC:
    case GREE_YAC1FB9:
    case GREE_YAP1F:
      this->mode_bits_ = GREE_MODE_BIT_LIGHT;
      break;
    case GREE_YX1FF:
    case GREE_YAG:
      this->mode_bits_ = GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH;
      break;
    case GREE_GENERIC:
    default:
      this->mode_bits_ = 0;
      break;
  }

  if (model == GREE_YAP1F) {
    this->swing_modes_ = {climate::CLIMATE_SWING_OFF};
  } else {
    this->swing_modes_ = {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH};
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

void GreeClimate::register_mode_bit_switch(uint8_t bit_mask, void *arg, void (*publish_state)(void *arg, bool state)) {
  if (this->mode_bit_switch_count_ < sizeof(this->mode_bit_switch_args_) / sizeof(this->mode_bit_switch_args_[0])) {
    const uint8_t index = this->mode_bit_switch_count_++;
    this->mode_bit_switch_masks_[index] = bit_mask;
    this->mode_bit_switch_args_[index] = arg;
    this->mode_bit_switch_publish_state_ = publish_state;
  }
}

void GreeClimate::publish_mode_bit_switches_() {
  for (uint8_t i = 0; i < this->mode_bit_switch_count_; i++) {
    this->mode_bit_switch_publish_state_(this->mode_bit_switch_args_[i],
                                         (this->mode_bits_ & this->mode_bit_switch_masks_[i]) != 0);
  }
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

  remote_state[0] = this->fan_speed_() | this->operation_mode_();
  remote_state[1] = this->temperature_();

  if (this->model_ == GREE_YAN) {
    remote_state[2] = GREE_MODE_BIT_LIGHT;  // bits 0..3 always 0000, bits 4..7 TURBO, LIGHT, HEALTH, X-FAN
    remote_state[3] = 0x50;                 // bits 4..7 always 0101
    remote_state[4] = this->vertical_swing_();
  }

  if (this->model_ == GREE_YX1FF || this->model_ == GREE_YAG) {
    remote_state[2] = GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH;
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
    remote_state[2] = GREE_MODE_BIT_LIGHT;  // bits 0..3 always 0000, bits 4..7 TURBO, LIGHT, HEALTH, X-FAN
    remote_state[3] = 0x50;                 // bits 4..7 always 0101
    remote_state[6] = 0x20;                 // YAA1FB, FAA1FB1, YB1F2 bits 4..7 always 0010

    if (this->vertical_swing_() == GREE_VDIR_SWING) {
      remote_state[0] |= (1 << 6);  // Enable swing by setting bit 6
    } else if (this->vertical_swing_() != GREE_VDIR_AUTO) {
      remote_state[5] = (remote_state[5] & 0xF0) | this->vertical_swing_();
    }
  }

  if (gree_has_mode_bits(this->model_)) {
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
    remote_state[7] = gree_yan_checksum(remote_state);
  } else if (this->model_ == GREE_YAG) {
    remote_state[7] = gree_yag_checksum(remote_state);
  } else {
    remote_state[7] = gree_standard_checksum(remote_state);
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

static bool gree_yap_read_byte(remote_base::RemoteReceiveData &data, uint8_t *value) {
  return gree_read_byte(data, GREE_YAP_BIT_MARK, GREE_YAP_ONE_SPACE, GREE_YAP_ZERO_SPACE, value);
}

bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (this->model_ == GREE_YAP1F) {
    uint8_t buffer[25] = {0};

    for (size_t pos = 0; pos < 24; pos += 8) {
      if (!data.expect_item(GREE_YAP_HEADER_MARK, GREE_YAP_HEADER_SPACE)) {
        return false;
      }

      for (size_t i = 0; i < 4; i++) {
        if (!gree_yap_read_byte(data, &buffer[pos + i])) {
          return false;
        }
      }

      if (!data.expect_item(GREE_YAP_BIT_MARK, GREE_YAP_ZERO_SPACE) ||
          !data.expect_item(GREE_YAP_BIT_MARK, GREE_YAP_ONE_SPACE) ||
          !data.expect_item(GREE_YAP_BIT_MARK, GREE_YAP_ZERO_SPACE)) {
        return false;
      }

      if (!data.expect_item(GREE_YAP_BIT_MARK, GREE_YAP_MESSAGE_SPACE)) {
        return false;
      }

      for (size_t i = 5; i < 9; i++) {
        if (!gree_yap_read_byte(data, &buffer[pos + i])) {
          return false;
        }
      }

      if (!data.expect_mark(GREE_YAP_BIT_MARK)) {
        return false;
      }

      if (pos + 8 < 24) {
        if (!data.expect_space(GREE_YAP_MESSAGE_SPACE)) {
          return false;
        }
      } else if (data.peek_space_at_most(1)) {
        // The transmitter may include a zero-length spacer after the trailing mark.
        // Real receiver captures can omit it, so consume it only when present.
        data.advance();
      }
    }

    const uint8_t checksum0 = gree_yap_checksum(buffer);
    if (buffer[8] != checksum0) {
      GREE_LOG_DECODE("Checksum 0 mismatch (got %02X expected %02X)", buffer[8], checksum0);
      return false;
    }

    const uint8_t checksum1 = gree_yap_checksum(buffer + 8);
    if (buffer[16] != checksum1) {
      GREE_LOG_DECODE("Checksum 1 mismatch (got %02X expected %02X)", buffer[16], checksum1);
      return false;
    }

    const bool power_on = (buffer[0] & GREE_MODE_ON) != 0;
    if (!power_on) {
      this->mode = climate::CLIMATE_MODE_OFF;
    } else {
      this->mode = gree_decode_mode(buffer[0] & 0x07);
    }

    // Temperature is encoded as (temp - 16) in the low nibble.
    this->target_temperature = float((buffer[1] & 0x0F) + GREE_TEMP_MIN);

    this->fan_mode = gree_decode_fan(buffer[0] & 0xF0, false);
    this->default_vertical_direction_ = VERTICAL_DIRECTION_AUTO;
    this->default_horizontal_direction_ = HORIZONTAL_DIRECTION_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;

    this->mode_bits_ = buffer[2] & 0xF0;
    this->publish_mode_bit_switches_();

    this->publish_state();
    return true;
  }

  uint8_t buffer[8] = {0};

  if (!data.expect_item(GREE_HEADER_MARK, gree_header_space(this->model_))) {
    return false;
  }

  for (size_t i = 0; i < 4; i++) {
    if (!gree_read_byte(data, GREE_BIT_MARK, GREE_ONE_SPACE, GREE_ZERO_SPACE, &buffer[i])) {
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
    if (!gree_read_byte(data, GREE_BIT_MARK, GREE_ONE_SPACE, GREE_ZERO_SPACE, &buffer[i])) {
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
    GREE_LOG_DECODE("Checksum mismatch (got %02X expected %02X)", buffer[7], expected_checksum);
    return false;
  }

  const bool power_on = (buffer[0] & GREE_MODE_ON) != 0;
  if (!power_on) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    this->mode = gree_decode_mode(buffer[0] & 0x07);
  }

  if (buffer[1] < GREE_TEMP_MIN || buffer[1] > GREE_TEMP_MAX) {
    GREE_LOG_DECODE("Unsupported temperature value: 0x%02X", buffer[1]);
    return false;
  }
  this->target_temperature = float(buffer[1]);

  if (this->model_ == GREE_YX1FF) {
    const uint8_t fan_speed = (buffer[2] & GREE_FAN_TURBO_BIT) != 0 ? GREE_FAN_TURBO : (buffer[0] & 0x30);
    this->fan_mode = gree_decode_fan(fan_speed, true);
    this->preset = (buffer[0] & GREE_PRESET_SLEEP_BIT) ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;
  } else {
    this->fan_mode = gree_decode_fan(buffer[0] & 0x30, false);
  }

  bool vertical_swing = false;
  bool horizontal_swing = false;
  bool supports_horizontal_swing = false;

  if (this->model_ == GREE_YAN || this->model_ == GREE_YX1FF) {
    const uint8_t vdir = buffer[4] & 0x0F;
    vertical_swing = vdir == GREE_VDIR_SWING;
    if (!vertical_swing) {
      if (!gree_is_fixed_vertical_direction(vdir)) {
        GREE_LOG_DECODE("Unsupported vertical direction value: 0x%02X", vdir);
        return false;
      }
      this->default_vertical_direction_ = static_cast<VerticalDirections>(vdir);
    }
  } else if (this->model_ == GREE_YAA || this->model_ == GREE_YAC || this->model_ == GREE_YAC1FB9) {
    vertical_swing = (buffer[0] & (1 << 6)) != 0;
    if (!vertical_swing) {
      const uint8_t vdir = buffer[5] & 0x0F;
      if (!gree_is_fixed_vertical_direction(vdir)) {
        GREE_LOG_DECODE("Unsupported vertical direction value: 0x%02X", vdir);
        return false;
      }
      this->default_vertical_direction_ = static_cast<VerticalDirections>(vdir);
    }
    if (this->model_ == GREE_YAC) {
      supports_horizontal_swing = true;
      const uint8_t hdir = (buffer[4] >> 4) & 0x0F;
      horizontal_swing = hdir == GREE_HDIR_SWING;
      if (!horizontal_swing) {
        if (!gree_is_fixed_horizontal_direction(hdir)) {
          GREE_LOG_DECODE("Unsupported horizontal direction value: 0x%02X", hdir);
          return false;
        }
        this->default_horizontal_direction_ = static_cast<HorizontalDirections>(hdir);
      }
    }
  } else if (this->model_ == GREE_YAG) {
    supports_horizontal_swing = true;
    const uint8_t vdir = buffer[4] & 0x0F;
    const uint8_t hdir = (buffer[4] >> 4) & 0x0F;
    vertical_swing = vdir == GREE_VDIR_SWING;
    horizontal_swing = hdir == GREE_HDIR_SWING;

    if (!vertical_swing) {
      if (!gree_is_fixed_vertical_direction(vdir)) {
        GREE_LOG_DECODE("Unsupported vertical direction value: 0x%02X", vdir);
        return false;
      }
      this->default_vertical_direction_ = static_cast<VerticalDirections>(vdir);
    }

    if (!horizontal_swing) {
      if (!gree_is_fixed_horizontal_direction(hdir)) {
        GREE_LOG_DECODE("Unsupported horizontal direction value: 0x%02X", hdir);
        return false;
      }
      this->default_horizontal_direction_ = static_cast<HorizontalDirections>(hdir);
    }
  }

  if (supports_horizontal_swing) {
    if (vertical_swing && horizontal_swing) {
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
    } else if (vertical_swing) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else if (horizontal_swing) {
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  } else {
    this->swing_mode = vertical_swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  }

  if (this->model_ == GREE_YX1FF) {
    this->mode_bits_ = (buffer[2] & 0xF0) & ~GREE_FAN_TURBO_BIT;
  } else {
    this->mode_bits_ = gree_has_mode_bits(this->model_) ? (buffer[2] & 0xF0) : 0;
  }
  this->publish_mode_bit_switches_();

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
      return this->default_horizontal_direction_;
  }
}

uint8_t GreeClimate::vertical_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_VDIR_SWING;
    default:
      return this->default_vertical_direction_;
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
