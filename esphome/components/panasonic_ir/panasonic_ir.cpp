#include "panasonic_ir.h"

#include <cmath>

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::panasonic_ir {

ESPHOME_LOG_TAG(TAG, "panasonic_ir.climate");

// How long after sending a frame an identical one is taken for its echo
static constexpr uint32_t ECHO_WINDOW_MS = 1000;

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// A mode not listed is sent as power off
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, static_cast<uint8_t>(Mode::AUTO)},
    {climate::CLIMATE_MODE_DRY, static_cast<uint8_t>(Mode::DRY)},
    {climate::CLIMATE_MODE_COOL, static_cast<uint8_t>(Mode::COOL)},
    {climate::CLIMATE_MODE_HEAT, static_cast<uint8_t>(Mode::HEAT)},
    {climate::CLIMATE_MODE_FAN_ONLY, static_cast<uint8_t>(Mode::FAN)},
};
// The minimum and maximum speeds are custom fan modes
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, FAN_SPEED_AUTO},
    {climate::CLIMATE_FAN_LOW, FAN_SPEED_LOW},
    {climate::CLIMATE_FAN_MEDIUM, FAN_SPEED_MEDIUM},
    {climate::CLIMATE_FAN_HIGH, FAN_SPEED_HIGH},
};

// In the order of the Model enum
PROGMEM_STRING_TABLE(ModelStrings, "DKE", "JKE", "LKE", "NKE", "CKP", "RKR", "PKE");

void PanasonicIrClimate::setup() {
  climate_ir::ClimateIR::setup();
  // The restored mode is the best guess of whether the unit is on
  this->ckp_is_on_ = this->mode != climate::CLIMATE_MODE_OFF;
  this->sync_vane_select_();
}

void PanasonicIrClimate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG, "  Model: %s", LOG_STR_ARG(ModelStrings::get_log_str(this->model_)));
}

void PanasonicIrClimate::set_vane_position(size_t index) {
  if (index > VANE_V_LOWEST) {
    ESP_LOGW(TAG, "Vane position %u out of range", static_cast<unsigned>(index));
    return;
  }
  // A vane position takes the unit out of frost protection, as a swing change does
  this->preset = climate::CLIMATE_PRESET_NONE;
  // The select only offers 0 to 5
  if (index == 0) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->vane_position_ = index;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  this->transmit_state();
  this->publish_state();
}

uint8_t PanasonicIrClimate::vane_v_() const {
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL)
    return VANE_V_AUTO;
  if (this->vane_position_ != 0)
    return this->vane_position_;
  // Blow down to heat from the bottom of the room, and up to cool from the top
  return this->mode == climate::CLIMATE_MODE_HEAT ? VANE_V_LOWEST : VANE_V_HIGHEST;
}

void PanasonicIrClimate::sync_vane_select_() {
#ifdef USE_SELECT
  if (this->vane_select_ == nullptr)
    return;
  const uint8_t vane = this->vane_v_();
  const size_t index = vane == VANE_V_AUTO ? 0 : vane;
  if (this->vane_select_->active_index().value_or(SIZE_MAX) != index)
    this->vane_select_->publish_state(index);
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
    // Choosing frost protection turns the unit on. transmit_state() sets the rest
    if (*call.get_preset() == climate::CLIMATE_PRESET_AWAY)
      this->mode = climate::CLIMATE_MODE_HEAT;
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
  return to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), FAN_SPEED_AUTO);
}

void PanasonicIrClimate::transmit_state() {
  // Frost protection is sent as the remote does, whatever else a call combined with the preset set, and the entity
  // shows what is sent
  const bool maintenance = this->mode != climate::CLIMATE_MODE_OFF &&
                           this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_AWAY;
  if (maintenance) {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->set_custom_fan_mode_(FAN_MODE_MAXIMUM);
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  }

  FrameParams params;
  params.model = this->model_;

  const auto mode = find_ir_code(MODES, this->mode, false);
  const bool power = mode.has_value();
  params.mode = power ? static_cast<Mode>(mode->code) : this->last_mode_;
  if (power)
    this->last_mode_ = params.mode;

  if (this->model_ == Model::CKP) {
    // No declarative power on CKP. The power bit toggles the unit, so only set it when the state changes
    params.power = (power != this->ckp_is_on_);
    this->ckp_is_on_ = power;
  } else {
    params.power = power;
  }

  params.temperature = maintenance
                           ? TEMP_MAINTENANCE
                           : static_cast<uint8_t>(lroundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX)));
  params.fan_speed = this->fan_speed_();
  params.vane_v = this->vane_v_();

  uint8_t state[STATE_LENGTH];
  encode_frame(params, state);
  memcpy(this->echo_, state + SECTION1_LENGTH, SECTION2_LENGTH);
  this->echo_sent_ms_ = millis();

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(CARRIER_FREQUENCY);
  // Two headers (mark + space) and footer marks, 16 timings per byte, and the gap
  data->reserve(2 * 3 + STATE_LENGTH * 16 + 1);

  data->item(HEADER_MARK, HEADER_SPACE);
  data->item_bytes<BIT_TIMINGS>(state, SECTION1_LENGTH);
  data->item(BIT_MARK, SECTION_GAP);
  data->item(HEADER_MARK, HEADER_SPACE);
  data->item_bytes<BIT_TIMINGS>(state + SECTION1_LENGTH, SECTION2_LENGTH);
  data->mark(BIT_MARK);

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

  // A receiver near the transmitter hears our own frame. Applying it would flip the CKP power toggle back and store
  // the vane position derived from the mode as one the user chose.
  if (millis() - this->echo_sent_ms_ < ECHO_WINDOW_MS &&
      memcmp(state + SECTION1_LENGTH, this->echo_, SECTION2_LENGTH) == 0) {
    ESP_LOGD(TAG, "Ignoring the echo of our own frame");
    this->echo_sent_ms_ -= ECHO_WINDOW_MS;  // only the first echo
    return true;
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
    this->mode = from_ir_code(MODES, static_cast<uint8_t>(params.mode), climate::CLIMATE_MODE_HEAT_COOL);
  }

  // The frost protection frame holds 8 C or 10 C, which is not a set-point of the normal range. Keep the one we had
  const bool maintenance = params.temperature < TEMP_MIN;
  this->preset = maintenance ? climate::CLIMATE_PRESET_AWAY : climate::CLIMATE_PRESET_NONE;
  // Fan only mode always sends a fixed temperature, so it says nothing about the set-point either
  if (params.mode != Mode::FAN && !maintenance)
    this->target_temperature = params.temperature;

  if (params.fan_speed == FAN_SPEED_MIN) {
    this->set_custom_fan_mode_(FAN_MODE_MINIMUM);
  } else if (params.fan_speed == FAN_SPEED_MAX) {
    this->set_custom_fan_mode_(FAN_MODE_MAXIMUM);
  } else {
    this->set_fan_mode_(from_ir_code(FANS, params.fan_speed, climate::CLIMATE_FAN_AUTO));
  }

  // Any other vane value is not one the remote sends, so it leaves the vane as it was
  if (params.vane_v == VANE_V_AUTO) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (params.vane_v >= VANE_V_HIGHEST && params.vane_v <= VANE_V_LOWEST) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->vane_position_ = params.vane_v;
  } else {
    ESP_LOGV(TAG, "Ignoring vane value %u", params.vane_v);
  }
  this->sync_vane_select_();

  this->publish_state();
}

}  // namespace esphome::panasonic_ir
