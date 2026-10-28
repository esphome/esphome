#include "gree.h"

#include <cmath>
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.climate");

static constexpr remote_base::BitTimings GREE_BIT_TIMINGS{
    .bit_mark = GREE_BIT_MARK, .one_space = GREE_ONE_SPACE, .zero_space = GREE_ZERO_SPACE};

static constexpr uint8_t GREE_SWING_AUTO_MASK = 0x40;
#ifdef USE_GREE_YAW1F
// The YAW1F second message is 00 00 00 A0 | 00 00 00 A0.
static constexpr uint8_t GREE_YAW1F_FOLLOWUP_BYTE = 0xA0;
#endif

// The mode, fan and feature mappings use if-else: a switch becomes a lookup table in RAM on ESP8266.
static constexpr uint8_t supported_feature_mask(Model model) {
  if (model == GREE_YB1FA)
    return GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN;
  if (model == GREE_YX1FF)
    return GREE_MODE_BIT_LIGHT;
  if (model == GREE_GENERIC || model == GREE_YAG || model == GREE_YAW1F)
    return 0;
  return GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH | GREE_MODE_BIT_XFAN;
}

static constexpr uint8_t default_feature_bits(Model model) {
  return supported_feature_mask(model) & GREE_MODE_BIT_LIGHT;
}

#ifdef USE_GREE_FAHRENHEIT
uint8_t fahrenheit_half_steps(float target_celsius) {
  if (!std::isfinite(target_celsius)) {
    return 0;
  }
  // Whole degrees shown at k = 0 and at the top of the range
  static constexpr int MIN_F = static_cast<int>(celsius_to_fahrenheit(GREE_TEMP_MIN));
  static constexpr int MAX_F = static_cast<int>(celsius_to_fahrenheit(GREE_TEMP_MAX));
  const int desired_f = static_cast<int>(roundf(clamp<float>(celsius_to_fahrenheit(target_celsius), MIN_F, MAX_F)));
  // The display at k is (10 * 32 + 18 * GREE_TEMP_MIN + 9 * k) / 10; pick the smallest k that reaches desired_f
  const int tenths_above_min = 10 * desired_f - (10 * 32 + 18 * GREE_TEMP_MIN);
  return static_cast<uint8_t>((tenths_above_min + 8) / 9);
}
#endif

struct GreeFrameSpaces {
  uint32_t header;
  uint32_t message;
};

static GreeFrameSpaces frame_spaces(Model model) {
  if (model == GREE_YAC1FB9 || model == GREE_YAW1F)
    return {GREE_YAC1FB9_HEADER_SPACE, GREE_YAC1FB9_MESSAGE_SPACE};
  return {GREE_HEADER_SPACE, GREE_MESSAGE_SPACE};
}

static void write_frame(remote_base::RemoteTransmitData *data, const GreeState &frame, const GreeFrameSpaces &spaces) {
  data->item(GREE_HEADER_MARK, spaces.header);
  data->item_bytes<GREE_BIT_TIMINGS>(frame.data(), 4);
  // Fixed 010 between the two halves, then the message gap
  data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);
  data->item(GREE_BIT_MARK, GREE_ONE_SPACE);
  data->item(GREE_BIT_MARK, GREE_ZERO_SPACE);
  data->item(GREE_BIT_MARK, spaces.message);
  data->item_bytes<GREE_BIT_TIMINGS>(frame.data() + 4, 4);
  data->mark(GREE_BIT_MARK);
}

#ifdef USE_GREE_RECEIVER
static constexpr uint8_t GREE_MODE_MASK = 0x07;
static constexpr uint8_t GREE_POWER_MASK = 0x08;
static constexpr uint8_t GREE_FAN_MASK = 0x30;
static constexpr uint8_t GREE_SLEEP_MASK = 0x80;
static constexpr uint8_t GREE_TEMP_MASK = 0x0F;
static constexpr uint8_t GREE_TIMER_HALF_HOUR_MASK = 0x10;
static constexpr uint8_t GREE_TIMER_TENS_HOUR_MASK = 0x60;
static constexpr uint8_t GREE_TIMER_HOURS_MASK = 0x0F;
static constexpr uint8_t GREE_BYTE5_FIXED_MASK = 0xB8;
static constexpr uint8_t GREE_BYTE5_FIXED_VALUE = 0x20;

static bool read_frame(remote_base::RemoteReceiveData &data, GreeState &frame, const GreeFrameSpaces &spaces) {
  return data.expect_item(GREE_HEADER_MARK, spaces.header) && data.expect_bytes<GREE_BIT_TIMINGS>(frame.data(), 4) &&
         data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) && data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE) &&
         data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) && data.expect_item(GREE_BIT_MARK, spaces.message) &&
         data.expect_bytes<GREE_BIT_TIMINGS>(frame.data() + 4, 4) && data.expect_mark(GREE_BIT_MARK);
}

#ifdef USE_GREE_YAW1F
static bool is_yaw1f_followup(const GreeState &state) {
  return state[3] == GREE_YAW1F_FOLLOWUP_BYTE && state[7] == GREE_YAW1F_FOLLOWUP_BYTE &&
         (state[0] | state[1] | state[2] | state[4] | state[5] | state[6]) == 0;
}
#endif

// YAC sends a 0x70 follow-up frame and YAW1F a fixed second message; neither carries climate state.
static bool is_auxiliary_frame(Model model, const GreeState &state) {
#ifdef USE_GREE_YAW1F
  if (model == GREE_YAW1F && is_yaw1f_followup(state))
    return true;
#endif
  return model == GREE_YAC && (state[3] & 0xF0) == 0x70;
}

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

static climate::ClimateMode decode_operation_mode(uint8_t mode) {
  if (mode == GREE_MODE_COOL)
    return climate::CLIMATE_MODE_COOL;
  if (mode == GREE_MODE_DRY)
    return climate::CLIMATE_MODE_DRY;
  if (mode == GREE_MODE_FAN)
    return climate::CLIMATE_MODE_FAN_ONLY;
  if (mode == GREE_MODE_HEAT)
    return climate::CLIMATE_MODE_HEAT;
  return climate::CLIMATE_MODE_HEAT_COOL;
}

// Use if-else rather than lookup tables to avoid RAM usage on ESP8266.
static climate::ClimateFanMode decode_fan_mode(Model model, uint8_t fan) {
  if (fan == GREE_FAN_AUTO)
    return climate::CLIMATE_FAN_AUTO;
  if (model == GREE_YX1FF) {
    if (fan == GREE_FAN_1)
      return climate::CLIMATE_FAN_QUIET;
    if (fan == GREE_FAN_2)
      return climate::CLIMATE_FAN_LOW;
    return climate::CLIMATE_FAN_MEDIUM;
  }
  if (fan == GREE_FAN_1)
    return climate::CLIMATE_FAN_LOW;
  if (fan == GREE_FAN_2)
    return climate::CLIMATE_FAN_MEDIUM;
  return climate::CLIMATE_FAN_HIGH;
}
#endif  // USE_GREE_RECEIVER

void GreeProtocol::encode(remote_base::RemoteTransmitData *data, const GreeState &state) const {
  const GreeFrameSpaces spaces = frame_spaces(this->model_);
  data->reserve(this->model_ == GREE_YAW1F ? 280 : 140);
  data->set_carrier_frequency(GREE_IR_FREQUENCY);

  write_frame(data, state, spaces);
#ifdef USE_GREE_YAW1F
  if (this->model_ == GREE_YAW1F) {
    // The YAW1F remote follows every command with a fixed second message.
    GreeState followup{};
    followup[3] = followup[7] = GREE_YAW1F_FOLLOWUP_BYTE;
    data->space(GREE_YAW1F_FOLLOWUP_SPACE);
    write_frame(data, followup, spaces);
  }
#endif
  data->space(0);
}

#ifdef USE_GREE_RECEIVER
optional<GreeState> GreeProtocol::decode(remote_base::RemoteReceiveData data) const {
  const GreeFrameSpaces spaces = frame_spaces(this->model_);
  GreeState state{};
  if (!read_frame(data, state, spaces) || !GreeProtocol::valid_checksum(state))
    return {};

#ifdef USE_GREE_YAW1F
  // Depending on receiver idle settings, YAW1F's ~40 ms second message may arrive in
  // the same capture. Validate it completely; never treat it as climate state.
  if (this->model_ == GREE_YAW1F && data.is_valid(1)) {
    GreeState followup{};
    if (!data.expect_space(GREE_YAW1F_FOLLOWUP_SPACE) || !read_frame(data, followup, spaces) ||
        !is_yaw1f_followup(followup)) {
      ESP_LOGV(TAG, "YAW1F frame dropped: second message invalid");
      return {};
    }
  }
#endif

  // A receiver normally appends one final idle space. Reject any extra pulses.
  if (data.is_valid(1) || (data.is_valid() && data.peek() > 0)) {
    ESP_LOGV(TAG, "Frame dropped: extra pulses follow it (receiver idle may be too long)");
    return {};
  }

  return state;
}
#endif  // USE_GREE_RECEIVER

uint8_t GreeProtocol::calculate_checksum(const GreeState &state) {
  uint8_t sum = 0x0A;
  for (uint8_t pos = 0; pos < 4; pos++)
    sum += state[pos] & 0x0F;
  for (uint8_t pos = 4; pos < GREE_STATE_FRAME_SIZE - 1; pos++)
    sum += state[pos] >> 4;
  return (sum & 0x0F) << 4;
}

#ifdef USE_GREE_RECEIVER
bool GreeProtocol::valid_checksum(const GreeState &state) {
  return (state[GREE_STATE_FRAME_SIZE - 1] & 0xF0) == GreeProtocol::calculate_checksum(state);
}
#endif

GreeState GreeClimateCodec::encode(Model model, const GreeClimateData &data) {
  GreeState state{0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00};
  const uint8_t vertical_swing = GreeClimateCodec::encode_vertical_swing(data.swing_mode);
  const uint8_t horizontal_swing = GreeClimateCodec::encode_horizontal_swing(data.swing_mode);

  state[0] =
      GreeClimateCodec::encode_fan_mode(model, data.fan_mode) | GreeClimateCodec::encode_operation_mode(data.mode);
  const uint8_t target_temperature = clamp<uint8_t>(data.target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX);
  state[1] = target_temperature - GREE_TEMP_MIN;
  state[2] = data.feature_bits & supported_feature_mask(model);
  if (model != GREE_GENERIC)
    state[3] = 0x50;

  if (model == GREE_YAN)
    state[4] = vertical_swing;

  if (model == GREE_YB1FA || model == GREE_YX1FF) {
    if (data.mode != climate::CLIMATE_MODE_OFF)
      state[2] |= GREE_MODEL_A_BIT;
    if (model == GREE_YX1FF && data.fan_mode == climate::CLIMATE_FAN_HIGH)
      state[2] |= GREE_FAN_TURBO_BIT;
    if (vertical_swing == GREE_VDIR_SWING) {
      state[0] |= GREE_SWING_AUTO_MASK;
      // YX1FF repeats the automatic swing value in both nibbles, while YB1FA only uses the vertical field.
      state[4] = model == GREE_YX1FF ? GREE_YX1FF_VANE_SWING : GREE_VDIR_SWING;
    }
  }

  if (model == GREE_YAG || model == GREE_YAW1F) {
    state[2] |= 0x60;
    state[5] = model == GREE_YAW1F ? 0x80 : 0x40;
    if (vertical_swing == GREE_VDIR_SWING || horizontal_swing == GREE_HDIR_SWING)
      state[0] |= GREE_SWING_AUTO_MASK;
  }

  // YAC, like YAG, keeps vertical swing in the low nibble of byte 4; the byte 0 swing flag alone is not enough.
  if (model == GREE_YAC || model == GREE_YAG || model == GREE_YAW1F)
    state[4] = vertical_swing | (horizontal_swing << 4);

  if (model == GREE_YAA || model == GREE_YAC || model == GREE_YAC1FB9) {
    state[6] = 0x20;
    if (vertical_swing == GREE_VDIR_SWING)
      state[0] |= GREE_SWING_AUTO_MASK;
  }

  if ((model == GREE_YX1FF || model == GREE_YAC1FB9) && data.preset == climate::CLIMATE_PRESET_SLEEP)
    state[0] |= GREE_PRESET_SLEEP_BIT;

  if (data.fahrenheit) {
    state[3] |= GREE_FAHRENHEIT_BIT;
    if (data.half_degree)
      state[3] |= GREE_TEMP_HALF_DEGREE_BIT;
  }

  state[GREE_STATE_FRAME_SIZE - 1] = GreeProtocol::calculate_checksum(state);
  return state;
}

#ifdef USE_GREE_RECEIVER
optional<GreeClimateData> GreeClimateCodec::decode(Model model, const GreeState &state) {
  if (model == GREE_YB1FA || model == GREE_YX1FF)
    return GreeClimateCodec::decode_model_a(model, state);
  return GreeClimateCodec::decode_legacy(model, state);
}
#endif  // USE_GREE_RECEIVER

uint8_t GreeClimateCodec::encode_operation_mode(climate::ClimateMode mode) {
  if (mode == climate::CLIMATE_MODE_COOL)
    return GREE_MODE_ON | GREE_MODE_COOL;
  if (mode == climate::CLIMATE_MODE_DRY)
    return GREE_MODE_ON | GREE_MODE_DRY;
  if (mode == climate::CLIMATE_MODE_HEAT)
    return GREE_MODE_ON | GREE_MODE_HEAT;
  if (mode == climate::CLIMATE_MODE_HEAT_COOL)
    return GREE_MODE_ON | GREE_MODE_AUTO;
  if (mode == climate::CLIMATE_MODE_FAN_ONLY)
    return GREE_MODE_ON | GREE_MODE_FAN;
  return GREE_MODE_OFF;
}

uint8_t GreeClimateCodec::encode_fan_mode(Model model, climate::ClimateFanMode fan_mode) {
  if (model == GREE_YX1FF) {
    if (fan_mode == climate::CLIMATE_FAN_QUIET)
      return GREE_FAN_1;
    if (fan_mode == climate::CLIMATE_FAN_LOW)
      return GREE_FAN_2;
    if (fan_mode == climate::CLIMATE_FAN_MEDIUM || fan_mode == climate::CLIMATE_FAN_HIGH)
      return GREE_FAN_3;
    return GREE_FAN_AUTO;
  }

  if (fan_mode == climate::CLIMATE_FAN_LOW)
    return GREE_FAN_1;
  if (fan_mode == climate::CLIMATE_FAN_MEDIUM)
    return GREE_FAN_2;
  if (fan_mode == climate::CLIMATE_FAN_HIGH)
    return GREE_FAN_3;
  return GREE_FAN_AUTO;
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

#ifdef USE_GREE_RECEIVER
optional<GreeClimateData> GreeClimateCodec::decode_legacy(Model model, const GreeState &state) {
  if (is_auxiliary_frame(model, state))
    return {};

  const uint8_t mode = state[0] & GREE_MODE_MASK;
  const bool power = state[0] & GREE_POWER_MASK;
  const uint8_t fan = state[0] & GREE_FAN_MASK;
  const uint8_t temperature = state[1] & GREE_TEMP_MASK;

  // Legacy GREE-branded remotes have several model and OEM variants. Decode the fields that are stable across the
  // protocol, but do not require the transmitter-side fixed bytes: physical Tadiran/Tosot/GREE remotes may populate
  // timer, display-temperature, I-Feel, WiFi, vane-position, or otherwise unused fields differently.
  if (mode > GREE_MODE_HEAT || temperature > GREE_TEMP_MAX - GREE_TEMP_MIN)
    return {};

  const uint8_t vertical = state[4] & 0x0F;
  const uint8_t horizontal = (state[4] >> 4) & 0x07;
  bool swing_vertical = false;
  bool swing_horizontal = false;

  switch (model) {
    case GREE_YAG:
    case GREE_YAW1F:
    case GREE_YAC:
      // On YAC1FB bit 6 does not reliably identify vertical swing; use vane fields for both axes.
      swing_vertical = is_valid_vertical_swing(true, vertical);
      swing_horizontal = horizontal == GREE_HDIR_SWING;
      break;
    case GREE_GENERIC:
    case GREE_YAN:
    case GREE_YAA:
    case GREE_YAC1FB9:
    default:
      // ESPHome's YAN transmitter stores automatic vertical swing in byte 4, while YAA/YAC1FB9 use bit 6.
      // Accept either representation so branded remotes using the standard GREE layout are handled as well.
      swing_vertical = (state[0] & GREE_SWING_AUTO_MASK) || is_valid_vertical_swing(true, vertical);
      break;
  }

  GreeClimateData data{
      .mode = climate::CLIMATE_MODE_OFF,
      .target_temperature = static_cast<uint8_t>(GREE_TEMP_MIN + temperature),
      .fan_mode = climate::CLIMATE_FAN_AUTO,
      .swing_mode = climate::CLIMATE_SWING_OFF,
      .preset = model == GREE_YAC1FB9 && (state[0] & GREE_SLEEP_MASK) ? climate::CLIMATE_PRESET_SLEEP
                                                                      : climate::CLIMATE_PRESET_NONE,
      .feature_bits = static_cast<uint8_t>(state[2] & supported_feature_mask(model)),
      .fahrenheit = static_cast<bool>(state[3] & GREE_FAHRENHEIT_BIT),
      .half_degree = static_cast<bool>(state[3] & GREE_TEMP_HALF_DEGREE_BIT),
  };

  if (swing_vertical && swing_horizontal) {
    data.swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (swing_vertical) {
    data.swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_horizontal) {
    data.swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  }

  if (power)
    data.mode = decode_operation_mode(mode);

  data.fan_mode = decode_fan_mode(model, fan);

  return data;
}

optional<GreeClimateData> GreeClimateCodec::decode_model_a(Model model, const GreeState &state) {
  const uint8_t mode = state[0] & GREE_MODE_MASK;
  const bool power = state[0] & GREE_POWER_MASK;
  const uint8_t fan = state[0] & GREE_FAN_MASK;
  const bool swing = state[0] & GREE_SWING_AUTO_MASK;
  const bool turbo = state[2] & GREE_FAN_TURBO_BIT;
  const uint8_t temperature = state[1] & GREE_TEMP_MASK;

  // Validate fields that are not represented by Climate where possible. Supported independent feature state is
  // returned separately so the parent can synchronize its feature switches.
  if (mode > GREE_MODE_HEAT || temperature > GREE_TEMP_MAX - GREE_TEMP_MIN || !is_valid_timer(state) ||
      static_cast<bool>(state[2] & GREE_MODEL_A_BIT) != power ||
      (state[3] & ~(GREE_FAHRENHEIT_BIT | GREE_TEMP_HALF_DEGREE_BIT)) != 0x50 || !is_valid_swing(model, state) ||
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
      .feature_bits = static_cast<uint8_t>(state[2] & supported_feature_mask(model)),
      .fahrenheit = static_cast<bool>(state[3] & GREE_FAHRENHEIT_BIT),
      .half_degree = static_cast<bool>(state[3] & GREE_TEMP_HALF_DEGREE_BIT),
  };

  if (power)
    data.mode = decode_operation_mode(mode);

  data.fan_mode = turbo ? climate::CLIMATE_FAN_HIGH : decode_fan_mode(model, fan);

  return data;
}
#endif  // USE_GREE_RECEIVER

void GreeClimate::set_model(Model model) {
  if (model == GREE_YAN || model == GREE_YB1FA || model == GREE_YX1FF) {
    // These remotes only expose a vertical swing control.
    this->swing_modes_.erase(climate::CLIMATE_SWING_HORIZONTAL);
    this->swing_modes_.erase(climate::CLIMATE_SWING_BOTH);
  }
  if (model == GREE_YX1FF)
    this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);
  if (model == GREE_YX1FF || model == GREE_YAC1FB9) {
    this->presets_.insert(climate::CLIMATE_PRESET_NONE);
    this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);
  }

  this->model_ = model;
  this->feature_bits_ = default_feature_bits(model);
}

void GreeClimate::set_feature_state(GreeFeature feature, bool enabled) {
  const uint8_t bit = gree_feature_bit(feature);
  if (!(supported_feature_mask(this->model_) & bit)) {
    ESP_LOGW(TAG, "GREE model %u does not support feature %u", static_cast<unsigned>(this->model_),
             static_cast<unsigned>(feature));
#ifdef USE_GREE_FEATURE_SWITCH
    this->publish_feature_state_(feature);
#endif
    return;
  }

  // YB1FA reports Turbo as effective HIGH fan. Keep the Climate and feature entities consistent when Turbo is enabled.
  const bool fan_changed = this->model_ == GREE_YB1FA && feature == GREE_FEATURE_TURBO && enabled &&
                           this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO) != climate::CLIMATE_FAN_HIGH;
  if (fan_changed)
    this->fan_mode = climate::CLIMATE_FAN_HIGH;

  assign_bits(this->feature_bits_, bit, enabled);
  this->transmit_state();
#ifdef USE_GREE_FEATURE_SWITCH
  this->publish_feature_state_(feature);
#endif
  if (fan_changed)
    this->publish_state();
}

bool GreeClimate::get_feature_state(GreeFeature feature) const {
  return this->feature_bits_ & gree_feature_bit(feature);
}

#ifdef USE_GREE_FEATURE_SWITCH
void GreeClimate::setup() {
  climate_ir::ClimateIR::setup();
  // Apply every restored switch, then send one frame. Models that receive feature state have restoring disabled,
  // so their switches show the current feature bits instead.
  bool restored = false;
  for (size_t feature = 0; feature < this->feature_switches_.size(); feature++) {
    auto *feature_switch = this->feature_switches_[feature];
    if (feature_switch == nullptr)
      continue;
    if (auto initial = feature_switch->get_initial_state_with_restore_mode()) {
      // What control() would hand the switch's apply body
      assign_bits(this->feature_bits_, gree_feature_bit(static_cast<GreeFeature>(feature)),
                  *initial != feature_switch->is_inverted());
      restored = true;
    }
    this->publish_feature_state_(static_cast<GreeFeature>(feature));
  }
  if (restored)
    this->transmit_state();
}

void GreeClimate::register_feature_switch(GreeFeature feature, switch_::Switch *feature_switch) {
  this->feature_switches_[static_cast<uint8_t>(feature)] = feature_switch;
}

void GreeClimate::publish_feature_state_(GreeFeature feature) {
  auto *feature_switch = this->feature_switches_[static_cast<uint8_t>(feature)];
  PUBLISH_STATE(feature_switch, this->get_feature_state(feature));
}

#ifdef USE_GREE_RECEIVER
void GreeClimate::publish_feature_states_() {
  for (size_t feature = 0; feature < this->feature_switches_.size(); feature++)
    this->publish_feature_state_(static_cast<GreeFeature>(feature));
}
#endif
#endif  // USE_GREE_FEATURE_SWITCH

void GreeClimate::control(const climate::ClimateCall &call) {
  // An explicit fan selection exits YB1FA Turbo. Disabling Turbo itself leaves the effective HIGH fan selected.
  const bool clear_turbo =
      this->model_ == GREE_YB1FA && call.get_fan_mode().has_value() && this->get_feature_state(GREE_FEATURE_TURBO);
  if (clear_turbo) {
    this->feature_bits_ &= ~GREE_MODE_BIT_TURBO;
#ifdef USE_GREE_FEATURE_SWITCH
    this->publish_feature_state_(GREE_FEATURE_TURBO);
#endif
  }
  climate_ir::ClimateIR::control(call);
}

void GreeClimate::transmit_state() {
  GreeClimateData climate_data{
      .mode = this->mode,
      .target_temperature =
          static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX))),
      .fan_mode = this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
      .swing_mode = this->swing_mode,
      .preset = this->preset.value_or(climate::CLIMATE_PRESET_NONE),
      .feature_bits = this->feature_bits_,
  };
#ifdef USE_GREE_FAHRENHEIT
  if (this->use_fahrenheit_) {
    const uint8_t half_steps = fahrenheit_half_steps(this->target_temperature);
    climate_data.target_temperature = GREE_TEMP_MIN + (half_steps >> 1);
    climate_data.fahrenheit = true;
    climate_data.half_degree = half_steps & 1;
  }
#endif
  const GreeState state = GreeClimateCodec::encode(this->model_, climate_data);

  auto transmit = this->transmitter_->transmit();
  GreeProtocol(this->model_).encode(transmit.get_data(), state);
  transmit.perform();
}

#ifdef USE_GREE_RECEIVER
bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto state = GreeProtocol(this->model_).decode(data);
  if (!state.has_value())
    return false;

  auto decoded = GreeClimateCodec::decode(this->model_, *state);
  if (!decoded.has_value()) {
    ESP_LOGD(TAG, "GREE frame rejected for model %u: %02X %02X %02X %02X %02X %02X %02X %02X",
             static_cast<unsigned>(this->model_), (*state)[0], (*state)[1], (*state)[2], (*state)[3], (*state)[4],
             (*state)[5], (*state)[6], (*state)[7]);
    return false;
  }

  this->mode = decoded->mode;
  const int half_steps = 2 * (decoded->target_temperature - GREE_TEMP_MIN) + decoded->half_degree;
  float target = GREE_TEMP_MIN + 0.5f * half_steps;
  // The panel shows (10 * 32 + 18 * GREE_TEMP_MIN + 9 * k) / 10 F; store that degree so it is sent back unchanged
  if (decoded->fahrenheit) {
    // The remote truncates the displayed Fahrenheit degree; retain that exact integer
    // through subsequent feature-only and fan-only transmissions.
    const int fahrenheit_degree = (10 * 32 + 18 * GREE_TEMP_MIN + 9 * half_steps) / 10;
    target = fahrenheit_to_celsius(static_cast<float>(fahrenheit_degree));
  }
  this->target_temperature = target;
  this->fan_mode = decoded->fan_mode;
  this->swing_mode = decoded->swing_mode;
  this->preset = decoded->preset;
  this->feature_bits_ = decoded->feature_bits;
#ifdef USE_GREE_FEATURE_SWITCH
  this->publish_feature_states_();
#endif
  this->publish_state();
  return true;
}
#endif  // USE_GREE_RECEIVER

}  // namespace esphome::gree
