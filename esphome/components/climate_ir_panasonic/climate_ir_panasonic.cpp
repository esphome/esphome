#include "climate_ir_panasonic.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::climate_ir_panasonic {

ESPHOME_LOG_TAG(TAG, "climate.climate_ir_panasonic");

// In the order of the Model enum
PROGMEM_STRING_TABLE(ModelStrings, "DKE", "JKE", "LKE", "NKE", "CKP", "RKR", "PKE");

void PanasonicIrClimate::setup() {
  climate_ir::ClimateIR::setup();
  this->sync_vane_select_();
}

void PanasonicIrClimate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG, "  Model: %s",
                LOG_STR_ARG(ModelStrings::get_log_str(static_cast<uint8_t>(this->model_), ModelStrings::LAST_INDEX)));
#ifdef USE_SELECT
  LOG_SELECT("  ", "Vane position", this->vane_select_);
#endif
}

void PanasonicIrClimate::set_vane_position(size_t index) {
  if (index == 0) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (index <= VANE_V_LOWEST) {
    this->vane_position_ = index;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  } else {
    return;
  }
  this->transmit_state();
  this->publish_state();
}

uint8_t PanasonicIrClimate::fixed_vane_() const {
  if (this->vane_position_ != 0)
    return this->vane_position_;
  // Blow down to heat from the bottom of the room, and up to cool from the top
  return this->mode == climate::CLIMATE_MODE_HEAT ? VANE_V_LOWEST : VANE_V_HIGHEST;
}

uint8_t PanasonicIrClimate::vane_v_() const {
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL || this->swing_mode == climate::CLIMATE_SWING_BOTH)
    return VANE_V_AUTO;
  return this->fixed_vane_();
}

void PanasonicIrClimate::sync_vane_select_() {
#ifdef USE_SELECT
  if (this->vane_select_ == nullptr)
    return;
  const uint8_t vane = this->vane_v_();
  this->vane_select_->publish_state(vane == VANE_V_AUTO ? 0 : vane);
#endif
}

void PanasonicIrClimate::control(const climate::ClimateCall &call) {
  // The base class only knows about the regular fan modes
  if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  } else if (call.get_fan_mode().has_value()) {
    this->clear_custom_fan_mode_();
  }

  // Frost protection is a mode of its own on the remote. Changing anything else takes the unit out of it
  const bool in_maintenance = this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_AWAY;
  if (call.get_preset().has_value()) {
    if (*call.get_preset() == climate::CLIMATE_PRESET_AWAY) {
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->set_custom_fan_mode_(FAN_MODE_MAXIMUM);
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    }
  } else if (in_maintenance &&
             (call.get_mode().has_value() || call.get_target_temperature().has_value() ||
              call.get_fan_mode().has_value() || call.has_custom_fan_mode() || call.get_swing_mode().has_value())) {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
  climate_ir::ClimateIR::control(call);
}

uint8_t PanasonicIrClimate::fan_speed_() const {
  if (this->has_custom_fan_mode()) {
    const auto custom_fan_mode = this->get_custom_fan_mode();
    if (custom_fan_mode == FAN_MODE_MINIMUM)
      return FAN_SPEED_MIN;
    if (custom_fan_mode == FAN_MODE_MAXIMUM)
      return FAN_SPEED_MAX;
  }
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      return FAN_SPEED_LOW;
    case climate::CLIMATE_FAN_MEDIUM:
      return FAN_SPEED_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:
      return FAN_SPEED_HIGH;
    default:
      return FAN_SPEED_AUTO;
  }
}

void PanasonicIrClimate::send_section_(const uint8_t *bytes, size_t len, remote_base::RemoteTransmitData *data,
                                       bool gap) {
  data->item(HEADER_MARK, HEADER_SPACE);
  for (size_t i = 0; i < len; i++) {
    // LSB first
    for (uint8_t bit = 0; bit < 8; bit++)
      data->item(BIT_MARK, (bytes[i] & (1 << bit)) ? BIT_ONE_SPACE : BIT_ZERO_SPACE);
  }
  data->mark(BIT_MARK);
  if (gap)
    data->space(SECTION_GAP);
}

void PanasonicIrClimate::transmit_state() {
  FrameParams params;
  params.model = this->model_;

  bool power = true;
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      params.mode = Mode::AUTO;
      break;
    case climate::CLIMATE_MODE_DRY:
      params.mode = Mode::DRY;
      break;
    case climate::CLIMATE_MODE_COOL:
      params.mode = Mode::COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      params.mode = Mode::HEAT;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      params.mode = Mode::FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power = false;
      params.mode = this->last_mode_;
      break;
  }
  if (power)
    this->last_mode_ = params.mode;

  if (this->model_ == Model::CKP) {
    // No declarative power on CKP. The power bit toggles the unit, so only set it when the state changes
    params.power = (power != this->ckp_is_on_);
    this->ckp_is_on_ = power;
  } else {
    params.power = power;
  }

  params.maintenance = power && this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_AWAY;
  params.temperature = static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX)));
  params.fan_speed = this->fan_speed_();

  params.vane_v = this->vane_v_();

  uint8_t state[STATE_LENGTH];
  encode_frame(params, state);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(CARRIER_FREQUENCY);
  data->reserve(2 * (2 * STATE_LENGTH * 8 + 6));

  this->send_section_(state, SECTION1_LENGTH, data, true);
  this->send_section_(state + SECTION1_LENGTH, SECTION2_LENGTH, data, false);

  transmit.perform();
  this->sync_vane_select_();
}

bool PanasonicIrClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state[STATE_LENGTH];
  if (!read_state(data, state))
    return false;

  FrameParams params;
  params.model = this->model_;
  if (!decode_frame(state, params)) {
    ESP_LOGD(TAG, "Received frame is not a valid Panasonic state");
    return false;
  }

  this->apply_received_(params);
  return true;
}

void PanasonicIrClimate::apply_received_(const FrameParams &params) {
  bool power = params.power;
  if (this->model_ == Model::CKP) {
    // The power bit is only a toggle
    if (params.power)
      this->ckp_is_on_ = !this->ckp_is_on_;
    power = this->ckp_is_on_;
  }

  this->last_mode_ = params.mode;
  if (!power) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (params.mode) {
      case Mode::DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case Mode::COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case Mode::HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case Mode::FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case Mode::AUTO:
      default:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  // The frost protection frame holds 8 C, which is not a set-point. Keep the one we had
  this->preset = params.maintenance ? climate::CLIMATE_PRESET_AWAY : climate::CLIMATE_PRESET_NONE;
  // Fan only mode always sends a fixed temperature, so it says nothing about the set-point either
  if (params.mode != Mode::FAN && !params.maintenance)
    this->target_temperature = params.temperature;

  switch (params.fan_speed) {
    case FAN_SPEED_MIN:
      this->set_custom_fan_mode_(FAN_MODE_MINIMUM);
      break;
    case FAN_SPEED_LOW:
      this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case FAN_SPEED_MEDIUM:
      this->set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case FAN_SPEED_HIGH:
      this->set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      break;
    case FAN_SPEED_MAX:
      this->set_custom_fan_mode_(FAN_MODE_MAXIMUM);
      break;
    case FAN_SPEED_AUTO:
    default:
      this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      break;
  }

  if (params.vane_v == VANE_V_AUTO) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    if (params.vane_v >= VANE_V_HIGHEST && params.vane_v <= VANE_V_LOWEST)
      this->vane_position_ = params.vane_v;
  }
  this->sync_vane_select_();

  this->publish_state();
}

}  // namespace esphome::climate_ir_panasonic
