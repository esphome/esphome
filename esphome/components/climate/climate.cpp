#include "climate.h"

namespace esphome {
namespace climate {

static const char *TAG = "climate";

void ClimateCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->mode_.has_value()) {
    const char *mode_s = climate_mode_to_string(*this->mode_);
    ESP_LOGD(TAG, "  Mode: %s", mode_s);
  }
  if (this->custom_fan_mode_.has_value()) {
    this->fan_mode_.reset();
    ESP_LOGD(TAG, " Custom Fan: %s", this->custom_fan_mode_.value().c_str());
  }
  if (this->fan_mode_.has_value()) {
    this->custom_fan_mode_.reset();
    const char *fan_mode_s = climate_fan_mode_to_string(*this->fan_mode_);
    ESP_LOGD(TAG, "  Fan: %s", fan_mode_s);
  }
  if (this->custom_preset_.has_value()) {
    this->preset_.reset();
    ESP_LOGD(TAG, " Custom Preset: %s", this->custom_preset_.value().c_str());
  }
  if (this->preset_.has_value()) {
    this->custom_preset_.reset();
    const char *preset_s = climate_preset_to_string(*this->preset_);
    ESP_LOGD(TAG, "  Preset: %s", preset_s);
  }
  if (this->swing_mode_.has_value()) {
    const char *swing_mode_s = climate_swing_mode_to_string(*this->swing_mode_);
    ESP_LOGD(TAG, "  Swing: %s", swing_mode_s);
  }
  if (this->target_temperature_.has_value()) {
    ESP_LOGD(TAG, "  Target Temperature: %.2f", *this->target_temperature_);
  }
  if (this->target_temperature_low_.has_value()) {
    ESP_LOGD(TAG, "  Target Temperature Low: %.2f", *this->target_temperature_low_);
  }
  if (this->target_temperature_high_.has_value()) {
    ESP_LOGD(TAG, "  Target Temperature High: %.2f", *this->target_temperature_high_);
  }
  if (this->away_.has_value()) {
    ESP_LOGD(TAG, "  Away Mode: %s", ONOFF(*this->away_));
  }
  this->parent_->control(*this);
}
void ClimateCall::validate_() {
  auto traits = this->parent_->get_traits();
  if (this->mode_.has_value()) {
    auto mode = *this->mode_;
    if (!traits.supports_mode(mode)) {
      ESP_LOGW(TAG, "  Mode %s is not supported by this device!", climate_mode_to_string(mode));
      this->mode_.reset();
    }
  }
  if (this->custom_fan_mode_.has_value()) {
    auto custom_fan_mode = *this->custom_fan_mode_;
    if (!traits.supports_custom_fan_mode(custom_fan_mode)) {
      ESP_LOGW(TAG, "  Fan Mode %s is not supported by this device!", custom_fan_mode.c_str());
      this->custom_fan_mode_.reset();
    }
  } else if (this->fan_mode_.has_value()) {
    auto fan_mode = *this->fan_mode_;
    if (!traits.supports_fan_mode(fan_mode)) {
      ESP_LOGW(TAG, "  Fan Mode %s is not supported by this device!", climate_fan_mode_to_string(fan_mode));
      this->fan_mode_.reset();
    }
  }
  if (this->custom_preset_.has_value()) {
    auto custom_preset = *this->custom_preset_;
    if (!traits.supports_custom_preset(custom_preset)) {
      ESP_LOGW(TAG, "  Preset %s is not supported by this device!", custom_preset.c_str());
      this->custom_preset_.reset();
    }
  } else if (this->preset_.has_value()) {
    auto preset = *this->preset_;
    if (!traits.supports_preset(preset)) {
      ESP_LOGW(TAG, "  Preset %s is not supported by this device!", climate_preset_to_string(preset));
      this->preset_.reset();
    }
  }
  if (this->swing_mode_.has_value()) {
    auto swing_mode = *this->swing_mode_;
    if (!traits.supports_swing_mode(swing_mode)) {
      ESP_LOGW(TAG, "  Swing Mode %s is not supported by this device!", climate_swing_mode_to_string(swing_mode));
      this->swing_mode_.reset();
    }
  }
  if (this->target_temperature_.has_value()) {
    auto target = *this->target_temperature_;
    if (traits.get_supports_two_point_target_temperature()) {
      ESP_LOGW(TAG, "  Cannot set target temperature for climate device "
                    "with two-point target temperature!");
      this->target_temperature_.reset();
    } else if (isnan(target)) {
      ESP_LOGW(TAG, "  Target temperature must not be NAN!");
      this->target_temperature_.reset();
    }
  }
  if (this->target_temperature_low_.has_value() || this->target_temperature_high_.has_value()) {
    if (!traits.get_supports_two_point_target_temperature()) {
      ESP_LOGW(TAG, "  Cannot set low/high target temperature for this device!");
      this->target_temperature_low_.reset();
      this->target_temperature_high_.reset();
    }
  }
  if (this->target_temperature_low_.has_value() && isnan(*this->target_temperature_low_)) {
    ESP_LOGW(TAG, "  Target temperature low must not be NAN!");
    this->target_temperature_low_.reset();
  }
  if (this->target_temperature_high_.has_value() && isnan(*this->target_temperature_high_)) {
    ESP_LOGW(TAG, "  Target temperature low must not be NAN!");
    this->target_temperature_high_.reset();
  }
  if (this->target_temperature_low_.has_value() && this->target_temperature_high_.has_value()) {
    float low = *this->target_temperature_low_;
    float high = *this->target_temperature_high_;
    if (low > high) {
      ESP_LOGW(TAG, "  Target temperature low %.2f must be smaller than target temperature high %.2f!", low, high);
      this->target_temperature_low_.reset();
      this->target_temperature_high_.reset();
    }
  }
  if (this->away_.has_value()) {
    if (!traits.get_supports_away()) {
      ESP_LOGW(TAG, "  Cannot set away mode for this device!");
      this->away_.reset();
    }
  }
}
ClimateCall &ClimateCall::set_mode(ClimateMode mode) {
  this->mode_ = mode;
  return *this;
}
ClimateCall &ClimateCall::set_mode(const std::string &mode) {
  if (str_equals_case_insensitive(mode, "OFF")) {
    this->set_mode(CLIMATE_MODE_OFF);
  } else if (str_equals_case_insensitive(mode, "AUTO")) {
    this->set_mode(CLIMATE_MODE_AUTO);
  } else if (str_equals_case_insensitive(mode, "COOL")) {
    this->set_mode(CLIMATE_MODE_COOL);
  } else if (str_equals_case_insensitive(mode, "HEAT")) {
    this->set_mode(CLIMATE_MODE_HEAT);
  } else if (str_equals_case_insensitive(mode, "FAN_ONLY")) {
    this->set_mode(CLIMATE_MODE_FAN_ONLY);
  } else if (str_equals_case_insensitive(mode, "DRY")) {
    this->set_mode(CLIMATE_MODE_DRY);
  } else if (str_equals_case_insensitive(mode, "HEAT_COOL")) {
    this->set_mode(CLIMATE_MODE_HEAT_COOL);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  }
  return *this;
}
ClimateCall &ClimateCall::set_fan_mode(ClimateFanMode fan_mode) {
  this->fan_mode_ = fan_mode;
  this->custom_fan_mode_.reset();
  return *this;
}
ClimateCall &ClimateCall::set_fan_mode(const std::string &fan_mode) {
  if (str_equals_case_insensitive(fan_mode, "ON")) {
    this->set_fan_mode(CLIMATE_FAN_ON);
  } else if (str_equals_case_insensitive(fan_mode, "OFF")) {
    this->set_fan_mode(CLIMATE_FAN_OFF);
  } else if (str_equals_case_insensitive(fan_mode, "AUTO")) {
    this->set_fan_mode(CLIMATE_FAN_AUTO);
  } else if (str_equals_case_insensitive(fan_mode, "LOW")) {
    this->set_fan_mode(CLIMATE_FAN_LOW);
  } else if (str_equals_case_insensitive(fan_mode, "MEDIUM")) {
    this->set_fan_mode(CLIMATE_FAN_MEDIUM);
  } else if (str_equals_case_insensitive(fan_mode, "HIGH")) {
    this->set_fan_mode(CLIMATE_FAN_HIGH);
  } else if (str_equals_case_insensitive(fan_mode, "MIDDLE")) {
    this->set_fan_mode(CLIMATE_FAN_MIDDLE);
  } else if (str_equals_case_insensitive(fan_mode, "FOCUS")) {
    this->set_fan_mode(CLIMATE_FAN_FOCUS);
  } else if (str_equals_case_insensitive(fan_mode, "DIFFUSE")) {
    this->set_fan_mode(CLIMATE_FAN_DIFFUSE);
  } else {
    auto custom_fan_modes = this->parent_->get_traits().get_supported_custom_fan_modes();
    if (std::find(custom_fan_modes.begin(), custom_fan_modes.end(), fan_mode) != custom_fan_modes.end()) {
      this->custom_fan_mode_ = fan_mode;
      this->fan_mode_.reset();
    } else {
      ESP_LOGW(TAG, "'%s' - Unrecognized fan mode %s", this->parent_->get_name().c_str(), fan_mode.c_str());
    }
  }
  return *this;
}
ClimateCall &ClimateCall::set_fan_mode(optional<std::string> fan_mode) {
  if (fan_mode.has_value()) {
    this->set_fan_mode(fan_mode.value());
  }
  return *this;
}
ClimateCall &ClimateCall::set_preset(ClimatePreset preset) {
  this->preset_ = preset;
  this->custom_preset_.reset();
  return *this;
}
ClimateCall &ClimateCall::set_preset(const std::string &preset) {
  if (str_equals_case_insensitive(preset, "ECO")) {
    this->set_preset(CLIMATE_PRESET_ECO);
  } else if (str_equals_case_insensitive(preset, "AWAY")) {
    this->set_preset(CLIMATE_PRESET_AWAY);
  } else if (str_equals_case_insensitive(preset, "BOOST")) {
    this->set_preset(CLIMATE_PRESET_BOOST);
  } else if (str_equals_case_insensitive(preset, "COMFORT")) {
    this->set_preset(CLIMATE_PRESET_COMFORT);
  } else if (str_equals_case_insensitive(preset, "HOME")) {
    this->set_preset(CLIMATE_PRESET_HOME);
  } else if (str_equals_case_insensitive(preset, "SLEEP")) {
    this->set_preset(CLIMATE_PRESET_SLEEP);
  } else if (str_equals_case_insensitive(preset, "ACTIVITY")) {
    this->set_preset(CLIMATE_PRESET_ACTIVITY);
  } else {
    auto custom_presets = this->parent_->get_traits().get_supported_custom_presets();
    if (std::find(custom_presets.begin(), custom_presets.end(), preset) != custom_presets.end()) {
      this->custom_preset_ = preset;
      this->preset_.reset();
    } else {
      ESP_LOGW(TAG, "'%s' - Unrecognized preset %s", this->parent_->get_name().c_str(), preset.c_str());
    }
  }
  return *this;
}
ClimateCall &ClimateCall::set_preset(optional<std::string> preset) {
  if (preset.has_value()) {
    this->set_preset(preset.value());
  }
  return *this;
}
ClimateCall &ClimateCall::set_swing_mode(ClimateSwingMode swing_mode) {
  this->swing_mode_ = swing_mode;
  return *this;
}
ClimateCall &ClimateCall::set_swing_mode(const std::string &swing_mode) {
  if (str_equals_case_insensitive(swing_mode, "OFF")) {
    this->set_swing_mode(CLIMATE_SWING_OFF);
  } else if (str_equals_case_insensitive(swing_mode, "BOTH")) {
    this->set_swing_mode(CLIMATE_SWING_BOTH);
  } else if (str_equals_case_insensitive(swing_mode, "VERTICAL")) {
    this->set_swing_mode(CLIMATE_SWING_VERTICAL);
  } else if (str_equals_case_insensitive(swing_mode, "HORIZONTAL")) {
    this->set_swing_mode(CLIMATE_SWING_HORIZONTAL);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized swing mode %s", this->parent_->get_name().c_str(), swing_mode.c_str());
  }
  return *this;
}

ClimateCall &ClimateCall::set_target_temperature(float target_temperature) {
  this->target_temperature_ = target_temperature;
  return *this;
}
ClimateCall &ClimateCall::set_target_temperature_low(float target_temperature_low) {
  this->target_temperature_low_ = target_temperature_low;
  return *this;
}
ClimateCall &ClimateCall::set_target_temperature_high(float target_temperature_high) {
  this->target_temperature_high_ = target_temperature_high;
  return *this;
}
const optional<ClimateMode> &ClimateCall::get_mode() const { return this->mode_; }
const optional<float> &ClimateCall::get_target_temperature() const { return this->target_temperature_; }
const optional<float> &ClimateCall::get_target_temperature_low() const { return this->target_temperature_low_; }
const optional<float> &ClimateCall::get_target_temperature_high() const { return this->target_temperature_high_; }
const optional<bool> &ClimateCall::get_away() const { return this->away_; }
const optional<ClimateFanMode> &ClimateCall::get_fan_mode() const { return this->fan_mode_; }
const optional<std::string> &ClimateCall::get_custom_fan_mode() const { return this->custom_fan_mode_; }
const optional<ClimatePreset> &ClimateCall::get_preset() const { return this->preset_; }
const optional<std::string> &ClimateCall::get_custom_preset() const { return this->custom_preset_; }
const optional<ClimateSwingMode> &ClimateCall::get_swing_mode() const { return this->swing_mode_; }
ClimateCall &ClimateCall::set_away(bool away) {
  this->away_ = away;
  return *this;
}
ClimateCall &ClimateCall::set_away(optional<bool> away) {
  this->away_ = away;
  return *this;
}
ClimateCall &ClimateCall::set_target_temperature_high(optional<float> target_temperature_high) {
  this->target_temperature_high_ = target_temperature_high;
  return *this;
}
ClimateCall &ClimateCall::set_target_temperature_low(optional<float> target_temperature_low) {
  this->target_temperature_low_ = target_temperature_low;
  return *this;
}
ClimateCall &ClimateCall::set_target_temperature(optional<float> target_temperature) {
  this->target_temperature_ = target_temperature;
  return *this;
}
ClimateCall &ClimateCall::set_mode(optional<ClimateMode> mode) {
  this->mode_ = mode;
  return *this;
}
ClimateCall &ClimateCall::set_fan_mode(optional<ClimateFanMode> fan_mode) {
  this->fan_mode_ = fan_mode;
  this->custom_fan_mode_.reset();
  return *this;
}
ClimateCall &ClimateCall::set_preset(optional<ClimatePreset> preset) {
  this->preset_ = preset;
  this->custom_preset_.reset();
  return *this;
}
ClimateCall &ClimateCall::set_swing_mode(optional<ClimateSwingMode> swing_mode) {
  this->swing_mode_ = swing_mode;
  return *this;
}

void Climate::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}

optional<ClimateDeviceRestoreState> Climate::restore_state_() {
  this->rtc_ = global_preferences.make_preference<ClimateDeviceRestoreState>(this->get_object_id_hash());
  ClimateDeviceRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}
void Climate::save_state_() {
  ClimateDeviceRestoreState state{};
  // initialize as zero to prevent random data on stack triggering erase
  memset(&state, 0, sizeof(ClimateDeviceRestoreState));

  state.mode = this->mode;
  auto traits = this->get_traits();
  if (traits.get_supports_two_point_target_temperature()) {
    state.target_temperature_low = this->target_temperature_low;
    state.target_temperature_high = this->target_temperature_high;
  } else {
    state.target_temperature = this->target_temperature;
  }
  if (traits.get_supports_away()) {
    state.away = this->away;
  }
  if (traits.get_supports_fan_modes() && fan_mode.has_value()) {
    state.uses_custom_fan_mode = false;
    state.fan_mode = this->fan_mode.value();
  }
  if (!traits.get_supported_custom_fan_modes().empty() && custom_fan_mode.has_value()) {
    state.uses_custom_fan_mode = true;
    auto &custom_fan_modes = traits.get_supported_custom_fan_modes();
    auto it = std::find(custom_fan_modes.begin(), custom_fan_modes.end(), this->custom_fan_mode.value());
    // only set custom fan mode if value exists, otherwise leave it as is
    if (it != custom_fan_modes.cend()) {
      state.custom_fan_mode = std::distance(custom_fan_modes.begin(), it);
    }
  }
  if (traits.get_supports_presets() && preset.has_value()) {
    state.uses_custom_preset = false;
    state.preset = this->preset.value();
  }
  if (!traits.get_supported_custom_presets().empty() && custom_preset.has_value()) {
    state.uses_custom_preset = true;
    auto custom_presets = traits.get_supported_custom_presets();
    auto it = std::find(custom_presets.begin(), custom_presets.end(), this->custom_preset.value());
    // only set custom preset if value exists, otherwise leave it as is
    if (it != custom_presets.cend()) {
      state.custom_preset = std::distance(custom_presets.begin(), it);
    }
  }
  if (traits.get_supports_swing_modes()) {
    state.swing_mode = this->swing_mode;
  }

  this->rtc_.save(&state);
}
void Climate::publish_state() {
  ESP_LOGD(TAG, "'%s' - Sending state:", this->name_.c_str());
  auto traits = this->get_traits();

  ESP_LOGD(TAG, "  Mode: %s", climate_mode_to_string(this->mode));
  if (traits.get_supports_action()) {
    ESP_LOGD(TAG, "  Action: %s", climate_action_to_string(this->action));
  }
  if (traits.get_supports_fan_modes() && this->fan_mode.has_value()) {
    ESP_LOGD(TAG, "  Fan Mode: %s", climate_fan_mode_to_string(this->fan_mode.value()));
  }
  if (!traits.get_supported_custom_fan_modes().empty() && this->custom_fan_mode.has_value()) {
    ESP_LOGD(TAG, "  Custom Fan Mode: %s", this->custom_fan_mode.value().c_str());
  }
  if (traits.get_supports_presets() && this->preset.has_value()) {
    ESP_LOGD(TAG, "  Preset: %s", climate_preset_to_string(this->preset.value()));
  }
  if (!traits.get_supported_custom_presets().empty() && this->custom_preset.has_value()) {
    ESP_LOGD(TAG, "  Custom Preset: %s", this->custom_preset.value().c_str());
  }
  if (traits.get_supports_swing_modes()) {
    ESP_LOGD(TAG, "  Swing Mode: %s", climate_swing_mode_to_string(this->swing_mode));
  }
  if (traits.get_supports_current_temperature()) {
    ESP_LOGD(TAG, "  Current Temperature: %.2f°C", this->current_temperature);
  }
  if (traits.get_supports_two_point_target_temperature()) {
    ESP_LOGD(TAG, "  Target Temperature: Low: %.2f°C High: %.2f°C", this->target_temperature_low,
             this->target_temperature_high);
  } else {
    ESP_LOGD(TAG, "  Target Temperature: %.2f°C", this->target_temperature);
  }
  if (traits.get_supports_away()) {
    ESP_LOGD(TAG, "  Away: %s", ONOFF(this->away));
  }

  // Send state to frontend
  this->state_callback_.call();
  // Save state
  this->save_state_();
}
uint32_t Climate::hash_base() { return 3104134496UL; }

ClimateTraits Climate::get_traits() {
  auto traits = this->traits();
  if (this->visual_min_temperature_override_.has_value()) {
    traits.set_visual_min_temperature(*this->visual_min_temperature_override_);
  }
  if (this->visual_max_temperature_override_.has_value()) {
    traits.set_visual_max_temperature(*this->visual_max_temperature_override_);
  }
  if (this->visual_temperature_step_override_.has_value()) {
    traits.set_visual_temperature_step(*this->visual_temperature_step_override_);
  }
  return traits;
}

void Climate::set_visual_min_temperature_override(float visual_min_temperature_override) {
  this->visual_min_temperature_override_ = visual_min_temperature_override;
}
void Climate::set_visual_max_temperature_override(float visual_max_temperature_override) {
  this->visual_max_temperature_override_ = visual_max_temperature_override;
}
void Climate::set_visual_temperature_step_override(float visual_temperature_step_override) {
  this->visual_temperature_step_override_ = visual_temperature_step_override;
}
Climate::Climate(const std::string &name) : Nameable(name) {}
Climate::Climate() : Climate("") {}
ClimateCall Climate::make_call() { return ClimateCall(this); }

ClimateCall ClimateDeviceRestoreState::to_call(Climate *climate) {
  auto call = climate->make_call();
  auto traits = climate->get_traits();
  call.set_mode(this->mode);
  if (traits.get_supports_two_point_target_temperature()) {
    call.set_target_temperature_low(this->target_temperature_low);
    call.set_target_temperature_high(this->target_temperature_high);
  } else {
    call.set_target_temperature(this->target_temperature);
  }
  if (traits.get_supports_away()) {
    call.set_away(this->away);
  }
  if (traits.get_supports_fan_modes() || !traits.get_supported_custom_fan_modes().empty()) {
    call.set_fan_mode(this->fan_mode);
  }
  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    call.set_preset(this->preset);
  }
  if (traits.get_supports_swing_modes()) {
    call.set_swing_mode(this->swing_mode);
  }
  return call;
}
void ClimateDeviceRestoreState::apply(Climate *climate) {
  auto traits = climate->get_traits();
  climate->mode = this->mode;
  if (traits.get_supports_two_point_target_temperature()) {
    climate->target_temperature_low = this->target_temperature_low;
    climate->target_temperature_high = this->target_temperature_high;
  } else {
    climate->target_temperature = this->target_temperature;
  }
  if (traits.get_supports_away()) {
    climate->away = this->away;
  }
  if (traits.get_supports_fan_modes() && !this->uses_custom_fan_mode) {
    climate->fan_mode = this->fan_mode;
  }
  if (!traits.get_supported_custom_fan_modes().empty() && this->uses_custom_fan_mode) {
    climate->custom_fan_mode = traits.get_supported_custom_fan_modes()[this->custom_fan_mode];
  }
  if (traits.get_supports_presets() && !this->uses_custom_preset) {
    climate->preset = this->preset;
  }
  if (!traits.get_supported_custom_presets().empty() && this->uses_custom_preset) {
    climate->custom_preset = traits.get_supported_custom_presets()[this->custom_preset];
  }
  if (!traits.get_supported_custom_presets().empty() && uses_custom_preset) {
    climate->custom_preset = traits.get_supported_custom_presets()[this->preset];
  }
  if (traits.get_supports_swing_modes()) {
    climate->swing_mode = this->swing_mode;
  }
  climate->publish_state();
}

}  // namespace climate
}  // namespace esphome
