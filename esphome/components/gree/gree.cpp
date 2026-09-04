#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree {

static const char *const TAG = "gree.climate";

static constexpr uint8_t GREE_MODE_MASK = 0x07;
static constexpr uint8_t GREE_POWER_MASK = 0x08;
static constexpr uint8_t GREE_FAN_MASK = 0x30;
static constexpr uint8_t GREE_SWING_AUTO_MASK = 0x40;
static constexpr uint8_t GREE_SLEEP_MASK = 0x80;
static constexpr uint8_t GREE_TEMP_MASK = 0x0F;
static constexpr uint8_t GREE_TIMER_HALF_HOUR_MASK = 0x10;
static constexpr uint8_t GREE_TIMER_TENS_HOUR_MASK = 0x60;
static constexpr uint8_t GREE_TIMER_HOURS_MASK = 0x0F;
static constexpr uint8_t GREE_BYTE5_FIXED_MASK = 0xB8;
static constexpr uint8_t GREE_BYTE5_FIXED_VALUE = 0x20;

static bool is_valid_timer(const GreeState &state) {
  const uint8_t timer_hours = state[2] & GREE_TIMER_HOURS_MASK;
  const uint8_t timer_tens_hours = (state[1] & GREE_TIMER_TENS_HOUR_MASK) >> 5;
  const bool timer_half_hour = state[1] & GREE_TIMER_HALF_HOUR_MASK;

  if (timer_hours > 9 || timer_tens_hours > 2)
    return false;
  if (timer_tens_hours < 2)
    return true;
  return timer_hours < 4 || (timer_hours == 4 && !timer_half_hour);
}

static bool is_valid_vertical_swing(bool automatic, uint8_t position) {
  if (automatic) {
    return position == GREE_VDIR_SWING || position == GREE_VDIR_SWING_DOWN || position == GREE_VDIR_SWING_MIDDLE ||
           position == GREE_VDIR_SWING_UP;
  }
  return position == GREE_VDIR_MANUAL || (position >= GREE_VDIR_UP && position <= GREE_VDIR_DOWN);
}

static bool is_valid_swing(Model model, const GreeState &state) {
  const bool automatic = state[0] & GREE_SWING_AUTO_MASK;
  const uint8_t vertical = state[4] & 0x0F;
  const uint8_t horizontal = state[4] >> 4;

  if (!is_valid_vertical_swing(automatic, vertical))
    return false;
  if (model == GREE_YX1FF)
    return horizontal == (automatic ? GREE_HDIR_SWING : GREE_HDIR_MANUAL);
  return horizontal == GREE_HDIR_MANUAL;
}

void GreeProtocol::encode(remote_base::RemoteTransmitData *data, const GreeState &state) const {
  data->reserve(140);
  data->set_carrier_frequency(GREE_IR_FREQUENCY);

  data->mark(GREE_HEADER_MARK);
  data->space(this->model_ == GREE_YAC1FB9 ? GREE_YAC1FB9_HEADER_SPACE : GREE_HEADER_SPACE);

  for (uint8_t pos = 0; pos < 4; pos++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(GREE_BIT_MARK);
      data->space(state[pos] & mask ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);
  data->item(GREE_BIT_MARK, GREE_ONE_SPACE);
  data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);

  data->mark(GREE_BIT_MARK);
  data->space(this->model_ == GREE_YAC1FB9 ? GREE_YAC1FB9_MESSAGE_SPACE : GREE_MESSAGE_SPACE);

  for (uint8_t pos = 4; pos < GREE_STATE_FRAME_SIZE; pos++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(GREE_BIT_MARK);
      data->space(state[pos] & mask ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->mark(GREE_BIT_MARK);
  data->space(0);
}

bool GreeProtocol::decode_bytes_(remote_base::RemoteReceiveData *data, GreeState *state, uint8_t offset) const {
  for (uint8_t pos = offset; pos < offset + 4; pos++) {
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data->expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
        value |= 1 << bit;
      } else if (!data->expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
        return false;
      }
    }
    (*state)[pos] = value;
  }
  return true;
}

optional<GreeState> GreeProtocol::decode(remote_base::RemoteReceiveData data) const {
  GreeState state{};

  const uint32_t header_space = this->model_ == GREE_YAC1FB9 ? GREE_YAC1FB9_HEADER_SPACE : GREE_HEADER_SPACE;
  const uint32_t message_space = this->model_ == GREE_YAC1FB9 ? GREE_YAC1FB9_MESSAGE_SPACE : GREE_MESSAGE_SPACE;

  if (!data.expect_item(GREE_HEADER_MARK, header_space) || !this->decode_bytes_(&data, &state, 0))
    return {};

  if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE) ||
      !data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, message_space) ||
      !this->decode_bytes_(&data, &state, 4) || !data.expect_mark(GREE_BIT_MARK)) {
    return {};
  }

  // A receiver normally appends one final idle space. Reject any additional pulse data.
  if (data.is_valid(1) || (data.is_valid() && data.peek() > 0) || !GreeProtocol::valid_checksum(state))
    return {};

  return state;
}

uint8_t GreeProtocol::calculate_checksum(const GreeState &state) {
  uint8_t sum = 0x0A;
  for (uint8_t pos = 0; pos < 4; pos++)
    sum += state[pos] & 0x0F;
  for (uint8_t pos = 4; pos < GREE_STATE_FRAME_SIZE - 1; pos++)
    sum += state[pos] >> 4;
  return (sum & 0x0F) << 4;
}

bool GreeProtocol::valid_checksum(const GreeState &state) {
  return (state[GREE_STATE_FRAME_SIZE - 1] & 0xF0) == GreeProtocol::calculate_checksum(state);
}

GreeState GreeClimateCodec::encode(Model model, const GreeClimateData &data, uint8_t mode_bits) {
  GreeState state{0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};

  state[0] =
      GreeClimateCodec::encode_fan_mode(model, data.fan_mode) | GreeClimateCodec::encode_operation_mode(data.mode);
  const uint8_t target_temperature = clamp<uint8_t>(data.target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX);
  state[1] = target_temperature - GREE_TEMP_MIN;

  if (model == GREE_YAN) {
    state[2] = 0x20;
    state[3] = 0x50;
    state[4] = GreeClimateCodec::encode_vertical_swing(data.swing_mode);
  }

  if (model == GREE_YB1FA || model == GREE_YX1FF) {
    state[2] = GREE_LIGHT_BIT;
    if (data.mode != climate::CLIMATE_MODE_OFF)
      state[2] |= GREE_MODEL_A_BIT;
    state[3] = 0x50;

    if (model == GREE_YX1FF && data.fan_mode == climate::CLIMATE_FAN_HIGH)
      state[2] |= GREE_FAN_TURBO_BIT;

    if (data.swing_mode == climate::CLIMATE_SWING_VERTICAL || data.swing_mode == climate::CLIMATE_SWING_BOTH) {
      state[0] |= GREE_SWING_AUTO_MASK;
      state[4] = GREE_VDIR_SWING;
      // YX1FF repeats the automatic swing value in both nibbles, while YB1FA only uses the vertical field.
      if (model == GREE_YX1FF)
        state[4] |= GREE_HDIR_SWING << 4;
    }

    if (model == GREE_YX1FF && data.preset == climate::CLIMATE_PRESET_SLEEP)
      state[0] |= GREE_PRESET_SLEEP_BIT;
  }

  if (model == GREE_YAG) {
    state[2] = 0x60;
    state[3] = 0x50;
    state[4] = GreeClimateCodec::encode_vertical_swing(data.swing_mode);
    state[5] = 0x40;

    if (GreeClimateCodec::encode_vertical_swing(data.swing_mode) == GREE_VDIR_SWING ||
        GreeClimateCodec::encode_horizontal_swing(data.swing_mode) == GREE_HDIR_SWING) {
      state[0] |= GREE_SWING_AUTO_MASK;
    }
  }

  if (model == GREE_YAC || model == GREE_YAG)
    state[4] |= GreeClimateCodec::encode_horizontal_swing(data.swing_mode) << 4;

  if (model == GREE_YAA || model == GREE_YAC || model == GREE_YAC1FB9) {
    state[2] = 0x20;
    state[3] = 0x50;
    state[6] = 0x20;

    if (GreeClimateCodec::encode_vertical_swing(data.swing_mode) == GREE_VDIR_SWING) {
      state[0] |= GREE_SWING_AUTO_MASK;
    } else if (GreeClimateCodec::encode_vertical_swing(data.swing_mode) != GREE_VDIR_AUTO) {
      state[5] = GreeClimateCodec::encode_vertical_swing(data.swing_mode);
    }
  }

  if (model == GREE_YAN || model == GREE_YAA || model == GREE_YAC || model == GREE_YAC1FB9)
    state[2] = (state[2] & 0x0F) | mode_bits;

  state[GREE_STATE_FRAME_SIZE - 1] = GreeProtocol::calculate_checksum(state);
  return state;
}

optional<GreeClimateData> GreeClimateCodec::decode(Model model, const GreeState &state) {
  if (model != GREE_YB1FA && model != GREE_YX1FF)
    return {};
  return GreeClimateCodec::decode_model_a(model, state);
}

uint8_t GreeClimateCodec::encode_operation_mode(climate::ClimateMode mode) {
  uint8_t operation_mode = GREE_MODE_ON;

  switch (mode) {
    case climate::CLIMATE_MODE_COOL:
      operation_mode |= GREE_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operation_mode |= GREE_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operation_mode |= GREE_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operation_mode |= GREE_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operation_mode |= GREE_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operation_mode = GREE_MODE_OFF;
      break;
  }

  return operation_mode;
}

uint8_t GreeClimateCodec::encode_fan_mode(Model model, climate::ClimateFanMode fan_mode) {
  if (model == GREE_YX1FF) {
    switch (fan_mode) {
      case climate::CLIMATE_FAN_QUIET:
        return GREE_FAN_1;
      case climate::CLIMATE_FAN_LOW:
        return GREE_FAN_2;
      case climate::CLIMATE_FAN_MEDIUM:
      case climate::CLIMATE_FAN_HIGH:
        return GREE_FAN_3;
      case climate::CLIMATE_FAN_AUTO:
      default:
        return GREE_FAN_AUTO;
    }
  }

  switch (fan_mode) {
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

uint8_t GreeClimateCodec::encode_horizontal_swing(climate::ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_HDIR_SWING;
    default:
      return GREE_HDIR_MANUAL;
  }
}

uint8_t GreeClimateCodec::encode_vertical_swing(climate::ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_VDIR_SWING;
    default:
      return GREE_VDIR_MANUAL;
  }
}

optional<GreeClimateData> GreeClimateCodec::decode_model_a(Model model, const GreeState &state) {
  if (!GreeProtocol::valid_checksum(state))
    return {};

  const uint8_t mode = state[0] & GREE_MODE_MASK;
  const bool power = state[0] & GREE_POWER_MASK;
  const uint8_t fan = state[0] & GREE_FAN_MASK;
  const bool swing = state[0] & GREE_SWING_AUTO_MASK;
  const bool turbo = state[2] & GREE_FAN_TURBO_BIT;
  const uint8_t temperature = state[1] & GREE_TEMP_MASK;

  // Timer, Turbo, Light, X-Fan, display temperature, I-Feel, and WiFi are independent features that are not
  // represented by Climate. Validate their encodings where possible, then ignore them when publishing state.
  if (mode > GREE_MODE_HEAT || temperature > GREE_TEMP_MAX - GREE_TEMP_MIN || !is_valid_timer(state) ||
      static_cast<bool>(state[2] & GREE_MODEL_A_BIT) != power || state[3] != 0x50 || !is_valid_swing(model, state) ||
      (state[5] & GREE_BYTE5_FIXED_MASK) != GREE_BYTE5_FIXED_VALUE || state[6] != 0x00 || (state[7] & 0x0F) != 0x00 ||
      (model == GREE_YX1FF && turbo && fan != GREE_FAN_3)) {
    return {};
  }

  GreeClimateData data{
      .mode = climate::CLIMATE_MODE_OFF,
      .target_temperature = static_cast<uint8_t>(GREE_TEMP_MIN + temperature),
      .fan_mode = climate::CLIMATE_FAN_AUTO,
      .swing_mode = swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF,
      .preset = model == GREE_YX1FF && state[0] & GREE_SLEEP_MASK ? climate::CLIMATE_PRESET_SLEEP
                                                                  : climate::CLIMATE_PRESET_NONE,
  };

  if (power) {
    switch (mode) {
      case GREE_MODE_AUTO:
        data.mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case GREE_MODE_COOL:
        data.mode = climate::CLIMATE_MODE_COOL;
        break;
      case GREE_MODE_DRY:
        data.mode = climate::CLIMATE_MODE_DRY;
        break;
      case GREE_MODE_FAN:
        data.mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case GREE_MODE_HEAT:
        data.mode = climate::CLIMATE_MODE_HEAT;
        break;
    }
  }

  if (turbo) {
    data.fan_mode = climate::CLIMATE_FAN_HIGH;
  } else if (model == GREE_YX1FF) {
    switch (fan) {
      case GREE_FAN_1:
        data.fan_mode = climate::CLIMATE_FAN_QUIET;
        break;
      case GREE_FAN_2:
        data.fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case GREE_FAN_3:
        data.fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case GREE_FAN_AUTO:
        data.fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
    }
  } else {
    switch (fan) {
      case GREE_FAN_1:
        data.fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case GREE_FAN_2:
        data.fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case GREE_FAN_3:
        data.fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
      case GREE_FAN_AUTO:
        data.fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
    }
  }

  return data;
}

climate::ClimateTraits GreeClimate::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  // ClimateIR unconditionally includes HEAT_COOL in the base mode set; remove it when heat is not supported.
  if (!this->supports_heat_) {
    auto modes = traits.get_supported_modes();
    modes.erase(climate::CLIMATE_MODE_HEAT_COOL);
    traits.set_supported_modes(modes);
  }
  return traits;
}

void GreeClimate::set_model(Model model) {
  if (model == GREE_YAN || model == GREE_YB1FA || model == GREE_YX1FF) {
    // These remotes only expose a vertical swing control.
    this->swing_modes_.erase(climate::CLIMATE_SWING_HORIZONTAL);
    this->swing_modes_.erase(climate::CLIMATE_SWING_BOTH);
  }
  if (model == GREE_YX1FF)
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);
  if (model == GREE_YX1FF) {
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);
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
  const GreeClimateData climate_data{
      .mode = this->mode,
      .target_temperature =
          static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX))),
      .fan_mode = this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
      .swing_mode = this->swing_mode,
      .preset = this->preset.value_or(climate::CLIMATE_PRESET_NONE),
  };
  const GreeState state = GreeClimateCodec::encode(this->model_, climate_data, this->mode_bits_);

  auto transmit = this->transmitter_->transmit();
  GreeProtocol(this->model_).encode(transmit.get_data(), state);
  transmit.perform();
}

bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto state = GreeProtocol(this->model_).decode(data);
  if (!state.has_value())
    return false;

  auto decoded = GreeClimateCodec::decode(this->model_, *state);
  if (!decoded.has_value()) {
    ESP_LOGV(TAG, "Received a valid GREE frame that is not supported by the selected model");
    return false;
  }

  this->mode = decoded->mode;
  this->target_temperature = decoded->target_temperature;
  this->fan_mode = decoded->fan_mode;
  this->swing_mode = decoded->swing_mode;
  this->preset = decoded->preset;
  this->publish_state();
  return true;
}

}  // namespace esphome::gree
