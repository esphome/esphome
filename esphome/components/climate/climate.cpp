#include "climate.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/macros.h"

namespace esphome::climate {

static const char *const TAG = "climate";

// Memory-efficient lookup tables
struct StringToUint8 {
  const char *str;
  const uint8_t value;
};

constexpr StringToUint8 CLIMATE_MODES_BY_STR[] = {
    {"OFF", CLIMATE_MODE_OFF},
    {"AUTO", CLIMATE_MODE_AUTO},
    {"COOL", CLIMATE_MODE_COOL},
    {"HEAT", CLIMATE_MODE_HEAT},
    {"FAN_ONLY", CLIMATE_MODE_FAN_ONLY},
    {"DRY", CLIMATE_MODE_DRY},
    {"HEAT_COOL", CLIMATE_MODE_HEAT_COOL},
};

constexpr StringToUint8 CLIMATE_FAN_MODES_BY_STR[] = {
    {"ON", CLIMATE_FAN_ON},         {"OFF", CLIMATE_FAN_OFF},       {"AUTO", CLIMATE_FAN_AUTO},
    {"LOW", CLIMATE_FAN_LOW},       {"MEDIUM", CLIMATE_FAN_MEDIUM}, {"HIGH", CLIMATE_FAN_HIGH},
    {"MIDDLE", CLIMATE_FAN_MIDDLE}, {"FOCUS", CLIMATE_FAN_FOCUS},   {"DIFFUSE", CLIMATE_FAN_DIFFUSE},
    {"QUIET", CLIMATE_FAN_QUIET},
};

constexpr StringToUint8 CLIMATE_PRESETS_BY_STR[] = {
    {"ECO", CLIMATE_PRESET_ECO},           {"AWAY", CLIMATE_PRESET_AWAY}, {"BOOST", CLIMATE_PRESET_BOOST},
    {"COMFORT", CLIMATE_PRESET_COMFORT},   {"HOME", CLIMATE_PRESET_HOME}, {"SLEEP", CLIMATE_PRESET_SLEEP},
    {"ACTIVITY", CLIMATE_PRESET_ACTIVITY}, {"NONE", CLIMATE_PRESET_NONE},
};

constexpr StringToUint8 CLIMATE_SWING_MODES_BY_STR[] = {
    {"OFF", CLIMATE_SWING_OFF},
    {"BOTH", CLIMATE_SWING_BOTH},
    {"VERTICAL", CLIMATE_SWING_VERTICAL},
    {"HORIZONTAL", CLIMATE_SWING_HORIZONTAL},
};

void ClimateCall::perform() {
  this->parent_->control_callback_.call(*this);
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->mode_.has_value()) {
    const LogString *mode_s = climate_mode_to_string(*this->mode_);
    ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(mode_s));
  }
  if (this->custom_fan_mode_ != nullptr) {
    this->fan_mode_.reset();
    ESP_LOGD(TAG, " Custom Fan: %s", this->custom_fan_mode_);
  }
  if (this->fan_mode_.has_value()) {
    this->custom_fan_mode_ = nullptr;
    const LogString *fan_mode_s = climate_fan_mode_to_string(*this->fan_mode_);
    ESP_LOGD(TAG, "  Fan: %s", LOG_STR_ARG(fan_mode_s));
  }
  if (this->custom_preset_ != nullptr) {
    this->preset_.reset();
    ESP_LOGD(TAG, " Custom Preset: %s", this->custom_preset_);
  }
  if (this->preset_.has_value()) {
    this->custom_preset_ = nullptr;
    const LogString *preset_s = climate_preset_to_string(*this->preset_);
    ESP_LOGD(TAG, "  Preset: %s", LOG_STR_ARG(preset_s));
  }
  if (this->swing_mode_.has_value()) {
    const LogString *swing_mode_s = climate_swing_mode_to_string(*this->swing_mode_);
    ESP_LOGD(TAG, "  Swing: %s", LOG_STR_ARG(swing_mode_s));
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
  if (this->target_humidity_.has_value()) {
    ESP_LOGD(TAG, "  Target Humidity: %.0f", *this->target_humidity_);
  }
  this->parent_->control(*this);
}

void ClimateCall::validate_() {
  auto traits = this->parent_->get_traits();
  if (this->mode_.has_value()) {
    auto mode = *this->mode_;
    if (!traits.supports_mode(mode)) {
      ESP_LOGW(TAG, "  Mode %s not supported", LOG_STR_ARG(climate_mode_to_string(mode)));
      this->mode_.reset();
    }
  }
  if (this->custom_fan_mode_ != nullptr) {
    if (!traits.supports_custom_fan_mode(this->custom_fan_mode_)) {
      ESP_LOGW(TAG, "  Fan Mode %s not supported", this->custom_fan_mode_);
      this->custom_fan_mode_ = nullptr;
    }
  } else if (this->fan_mode_.has_value()) {
    auto fan_mode = *this->fan_mode_;
    if (!traits.supports_fan_mode(fan_mode)) {
      ESP_LOGW(TAG, "  Fan Mode %s not supported", LOG_STR_ARG(climate_fan_mode_to_string(fan_mode)));
      this->fan_mode_.reset();
    }
  }
  if (this->custom_preset_ != nullptr) {
    if (!traits.supports_custom_preset(this->custom_preset_)) {
      ESP_LOGW(TAG, "  Preset %s not supported", this->custom_preset_);
      this->custom_preset_ = nullptr;
    }
  } else if (this->preset_.has_value()) {
    auto preset = *this->preset_;
    if (!traits.supports_preset(preset)) {
      ESP_LOGW(TAG, "  Preset %s not supported", LOG_STR_ARG(climate_preset_to_string(preset)));
      this->preset_.reset();
    }
  }
  if (this->swing_mode_.has_value()) {
    auto swing_mode = *this->swing_mode_;
    if (!traits.supports_swing_mode(swing_mode)) {
      ESP_LOGW(TAG, "  Swing Mode %s not supported", LOG_STR_ARG(climate_swing_mode_to_string(swing_mode)));
      this->swing_mode_.reset();
    }
  }
  if (this->target_temperature_.has_value()) {
    auto target = *this->target_temperature_;
    if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                                 CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
      ESP_LOGW(TAG, "  Cannot set target temperature for climate device "
                    "with two-point target temperature");
      this->target_temperature_.reset();
    } else if (std::isnan(target)) {
      ESP_LOGW(TAG, "  Target temperature must not be NAN");
      this->target_temperature_.reset();
    }
  }
  if (this->target_temperature_low_.has_value() || this->target_temperature_high_.has_value()) {
    if (!traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                                  CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
      ESP_LOGW(TAG, "  Cannot set low/high target temperature");
      this->target_temperature_low_.reset();
      this->target_temperature_high_.reset();
    }
  }
  if (this->target_temperature_low_.has_value() && std::isnan(*this->target_temperature_low_)) {
    ESP_LOGW(TAG, "  Target temperature low must not be NAN");
    this->target_temperature_low_.reset();
  }
  if (this->target_temperature_high_.has_value() && std::isnan(*this->target_temperature_high_)) {
    ESP_LOGW(TAG, "  Target temperature high must not be NAN");
    this->target_temperature_high_.reset();
  }
  if (this->target_temperature_low_.has_value() && this->target_temperature_high_.has_value()) {
    float low = *this->target_temperature_low_;
    float high = *this->target_temperature_high_;
    if (low > high) {
      ESP_LOGW(TAG, "  Target temperature low %.2f must be less than target temperature high %.2f", low, high);
      this->target_temperature_low_.reset();
      this->target_temperature_high_.reset();
    }
  }
}

ClimateCall &ClimateCall::set_mode(ClimateMode mode) {
  this->mode_ = mode;
  return *this;
}

ClimateCall &ClimateCall::set_mode(const std::string &mode) {
  for (const auto &mode_entry : CLIMATE_MODES_BY_STR) {
    if (str_equals_case_insensitive(mode, mode_entry.str)) {
      this->set_mode(static_cast<ClimateMode>(mode_entry.value));
      return *this;
    }
  }
  ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  return *this;
}

ClimateCall &ClimateCall::set_fan_mode(ClimateFanMode fan_mode) {
  this->fan_mode_ = fan_mode;
  this->custom_fan_mode_ = nullptr;
  return *this;
}

ClimateCall &ClimateCall::set_fan_mode(const char *custom_fan_mode) {
  // Check if it's a standard enum mode first
  for (const auto &mode_entry : CLIMATE_FAN_MODES_BY_STR) {
    if (str_equals_case_insensitive(custom_fan_mode, mode_entry.str)) {
      return this->set_fan_mode(static_cast<ClimateFanMode>(mode_entry.value));
    }
  }
  // Find the matching pointer from parent climate device
  if (const char *mode_ptr = this->parent_->find_custom_fan_mode_(custom_fan_mode)) {
    this->custom_fan_mode_ = mode_ptr;
    this->fan_mode_.reset();
    return *this;
  }
  ESP_LOGW(TAG, "'%s' - Unrecognized fan mode %s", this->parent_->get_name().c_str(), custom_fan_mode);
  return *this;
}

ClimateCall &ClimateCall::set_fan_mode(const std::string &fan_mode) { return this->set_fan_mode(fan_mode.c_str()); }

ClimateCall &ClimateCall::set_fan_mode(optional<std::string> fan_mode) {
  if (fan_mode.has_value()) {
    this->set_fan_mode(fan_mode.value());
  }
  return *this;
}

ClimateCall &ClimateCall::set_preset(ClimatePreset preset) {
  this->preset_ = preset;
  this->custom_preset_ = nullptr;
  return *this;
}

ClimateCall &ClimateCall::set_preset(const char *custom_preset) {
  // Check if it's a standard enum preset first
  for (const auto &preset_entry : CLIMATE_PRESETS_BY_STR) {
    if (str_equals_case_insensitive(custom_preset, preset_entry.str)) {
      return this->set_preset(static_cast<ClimatePreset>(preset_entry.value));
    }
  }
  // Find the matching pointer from parent climate device
  if (const char *preset_ptr = this->parent_->find_custom_preset_(custom_preset)) {
    this->custom_preset_ = preset_ptr;
    this->preset_.reset();
    return *this;
  }
  ESP_LOGW(TAG, "'%s' - Unrecognized preset %s", this->parent_->get_name().c_str(), custom_preset);
  return *this;
}

ClimateCall &ClimateCall::set_preset(const std::string &preset) { return this->set_preset(preset.c_str()); }

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
  for (const auto &mode_entry : CLIMATE_SWING_MODES_BY_STR) {
    if (str_equals_case_insensitive(swing_mode, mode_entry.str)) {
      this->set_swing_mode(static_cast<ClimateSwingMode>(mode_entry.value));
      return *this;
    }
  }
  ESP_LOGW(TAG, "'%s' - Unrecognized swing mode %s", this->parent_->get_name().c_str(), swing_mode.c_str());
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

ClimateCall &ClimateCall::set_target_humidity(float target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}

const optional<float> &ClimateCall::get_target_temperature() const { return this->target_temperature_; }
const optional<float> &ClimateCall::get_target_temperature_low() const { return this->target_temperature_low_; }
const optional<float> &ClimateCall::get_target_temperature_high() const { return this->target_temperature_high_; }
const optional<float> &ClimateCall::get_target_humidity() const { return this->target_humidity_; }

const optional<ClimateMode> &ClimateCall::get_mode() const { return this->mode_; }
const optional<ClimateFanMode> &ClimateCall::get_fan_mode() const { return this->fan_mode_; }
const optional<ClimateSwingMode> &ClimateCall::get_swing_mode() const { return this->swing_mode_; }
const optional<ClimatePreset> &ClimateCall::get_preset() const { return this->preset_; }

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

ClimateCall &ClimateCall::set_target_humidity(optional<float> target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}

ClimateCall &ClimateCall::set_mode(optional<ClimateMode> mode) {
  this->mode_ = mode;
  return *this;
}

ClimateCall &ClimateCall::set_fan_mode(optional<ClimateFanMode> fan_mode) {
  this->fan_mode_ = fan_mode;
  this->custom_fan_mode_ = nullptr;
  return *this;
}

ClimateCall &ClimateCall::set_preset(optional<ClimatePreset> preset) {
  this->preset_ = preset;
  this->custom_preset_ = nullptr;
  return *this;
}

ClimateCall &ClimateCall::set_swing_mode(optional<ClimateSwingMode> swing_mode) {
  this->swing_mode_ = swing_mode;
  return *this;
}

void Climate::add_on_state_callback(std::function<void(Climate &)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void Climate::add_on_control_callback(std::function<void(ClimateCall &)> &&callback) {
  this->control_callback_.add(std::move(callback));
}

// Random 32bit value; If this changes existing restore preferences are invalidated
static const uint32_t RESTORE_STATE_VERSION = 0x848EA6ADUL;

optional<ClimateDeviceRestoreState> Climate::restore_state_() {
  this->rtc_ = global_preferences->make_preference<ClimateDeviceRestoreState>(this->get_preference_hash() ^
                                                                              RESTORE_STATE_VERSION);
  ClimateDeviceRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}

void Climate::save_state_() {
#if (defined(USE_ESP_IDF) || (defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 0, 0))) && \
    !defined(CLANG_TIDY)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#define TEMP_IGNORE_MEMACCESS
#endif
  ClimateDeviceRestoreState state{};
  // initialize as zero to prevent random data on stack triggering erase
  memset(&state, 0, sizeof(ClimateDeviceRestoreState));
#ifdef TEMP_IGNORE_MEMACCESS
#pragma GCC diagnostic pop
#undef TEMP_IGNORE_MEMACCESS
#endif

  state.mode = this->mode;
  auto traits = this->get_traits();
  if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    state.target_temperature_low = this->target_temperature_low;
    state.target_temperature_high = this->target_temperature_high;
  } else {
    state.target_temperature = this->target_temperature;
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    state.target_humidity = this->target_humidity;
  }
  if (traits.get_supports_fan_modes() && fan_mode.has_value()) {
    state.uses_custom_fan_mode = false;
    state.fan_mode = this->fan_mode.value();
  }
  if (!traits.get_supported_custom_fan_modes().empty() && this->has_custom_fan_mode()) {
    state.uses_custom_fan_mode = true;
    const auto &supported = traits.get_supported_custom_fan_modes();
    // std::vector maintains insertion order
    size_t i = 0;
    for (const char *mode : supported) {
      if (strcmp(mode, this->custom_fan_mode_) == 0) {
        state.custom_fan_mode = i;
        break;
      }
      i++;
    }
  }
  if (traits.get_supports_presets() && preset.has_value()) {
    state.uses_custom_preset = false;
    state.preset = this->preset.value();
  }
  if (!traits.get_supported_custom_presets().empty() && this->has_custom_preset()) {
    state.uses_custom_preset = true;
    const auto &supported = traits.get_supported_custom_presets();
    // std::vector maintains insertion order
    size_t i = 0;
    for (const char *preset : supported) {
      if (strcmp(preset, this->custom_preset_) == 0) {
        state.custom_preset = i;
        break;
      }
      i++;
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

  ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(climate_mode_to_string(this->mode)));
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    ESP_LOGD(TAG, "  Action: %s", LOG_STR_ARG(climate_action_to_string(this->action)));
  }
  if (traits.get_supports_fan_modes() && this->fan_mode.has_value()) {
    ESP_LOGD(TAG, "  Fan Mode: %s", LOG_STR_ARG(climate_fan_mode_to_string(this->fan_mode.value())));
  }
  if (!traits.get_supported_custom_fan_modes().empty() && this->has_custom_fan_mode()) {
    ESP_LOGD(TAG, "  Custom Fan Mode: %s", this->custom_fan_mode_);
  }
  if (traits.get_supports_presets() && this->preset.has_value()) {
    ESP_LOGD(TAG, "  Preset: %s", LOG_STR_ARG(climate_preset_to_string(this->preset.value())));
  }
  if (!traits.get_supported_custom_presets().empty() && this->has_custom_preset()) {
    ESP_LOGD(TAG, "  Custom Preset: %s", this->custom_preset_);
  }
  if (traits.get_supports_swing_modes()) {
    ESP_LOGD(TAG, "  Swing Mode: %s", LOG_STR_ARG(climate_swing_mode_to_string(this->swing_mode)));
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)) {
    ESP_LOGD(TAG, "  Current Temperature: %.2f°C", this->current_temperature);
  }
  if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    ESP_LOGD(TAG, "  Target Temperature: Low: %.2f°C High: %.2f°C", this->target_temperature_low,
             this->target_temperature_high);
  } else {
    ESP_LOGD(TAG, "  Target Temperature: %.2f°C", this->target_temperature);
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)) {
    ESP_LOGD(TAG, "  Current Humidity: %.0f%%", this->current_humidity);
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    ESP_LOGD(TAG, "  Target Humidity: %.0f%%", this->target_humidity);
  }

  // Send state to frontend
  this->state_callback_.call(*this);
#if defined(USE_CLIMATE) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_climate_update(this);
#endif
  // Save state
  this->save_state_();
}

ClimateTraits Climate::get_traits() {
  auto traits = this->traits();
  if (this->visual_min_temperature_override_.has_value()) {
    traits.set_visual_min_temperature(*this->visual_min_temperature_override_);
  }
  if (this->visual_max_temperature_override_.has_value()) {
    traits.set_visual_max_temperature(*this->visual_max_temperature_override_);
  }
  if (this->visual_target_temperature_step_override_.has_value()) {
    traits.set_visual_target_temperature_step(*this->visual_target_temperature_step_override_);
    traits.set_visual_current_temperature_step(*this->visual_current_temperature_step_override_);
  }
  if (this->visual_min_humidity_override_.has_value()) {
    traits.set_visual_min_humidity(*this->visual_min_humidity_override_);
  }
  if (this->visual_max_humidity_override_.has_value()) {
    traits.set_visual_max_humidity(*this->visual_max_humidity_override_);
  }

  return traits;
}

void Climate::set_visual_min_temperature_override(float visual_min_temperature_override) {
  this->visual_min_temperature_override_ = visual_min_temperature_override;
}

void Climate::set_visual_max_temperature_override(float visual_max_temperature_override) {
  this->visual_max_temperature_override_ = visual_max_temperature_override;
}

void Climate::set_visual_temperature_step_override(float target, float current) {
  this->visual_target_temperature_step_override_ = target;
  this->visual_current_temperature_step_override_ = current;
}

void Climate::set_visual_min_humidity_override(float visual_min_humidity_override) {
  this->visual_min_humidity_override_ = visual_min_humidity_override;
}

void Climate::set_visual_max_humidity_override(float visual_max_humidity_override) {
  this->visual_max_humidity_override_ = visual_max_humidity_override;
}

ClimateCall Climate::make_call() { return ClimateCall(this); }

ClimateCall ClimateDeviceRestoreState::to_call(Climate *climate) {
  auto call = climate->make_call();
  auto traits = climate->get_traits();
  call.set_mode(this->mode);
  if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    call.set_target_temperature_low(this->target_temperature_low);
    call.set_target_temperature_high(this->target_temperature_high);
  } else {
    call.set_target_temperature(this->target_temperature);
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    call.set_target_humidity(this->target_humidity);
  }
  if (this->uses_custom_fan_mode) {
    if (this->custom_fan_mode < traits.get_supported_custom_fan_modes().size()) {
      call.fan_mode_.reset();
      call.custom_fan_mode_ = traits.get_supported_custom_fan_modes()[this->custom_fan_mode];
    }
  } else if (traits.supports_fan_mode(this->fan_mode)) {
    call.set_fan_mode(this->fan_mode);
  }
  if (this->uses_custom_preset) {
    if (this->custom_preset < traits.get_supported_custom_presets().size()) {
      call.preset_.reset();
      call.custom_preset_ = traits.get_supported_custom_presets()[this->custom_preset];
    }
  } else if (traits.supports_preset(this->preset)) {
    call.set_preset(this->preset);
  }
  if (traits.supports_swing_mode(this->swing_mode)) {
    call.set_swing_mode(this->swing_mode);
  }
  return call;
}

void ClimateDeviceRestoreState::apply(Climate *climate) {
  auto traits = climate->get_traits();
  climate->mode = this->mode;
  if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    climate->target_temperature_low = this->target_temperature_low;
    climate->target_temperature_high = this->target_temperature_high;
  } else {
    climate->target_temperature = this->target_temperature;
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    climate->target_humidity = this->target_humidity;
  }
  if (this->uses_custom_fan_mode) {
    if (this->custom_fan_mode < traits.get_supported_custom_fan_modes().size()) {
      climate->fan_mode.reset();
      climate->custom_fan_mode_ = traits.get_supported_custom_fan_modes()[this->custom_fan_mode];
    }
  } else if (traits.supports_fan_mode(this->fan_mode)) {
    climate->fan_mode = this->fan_mode;
    climate->clear_custom_fan_mode_();
  }
  if (this->uses_custom_preset) {
    if (this->custom_preset < traits.get_supported_custom_presets().size()) {
      climate->preset.reset();
      climate->custom_preset_ = traits.get_supported_custom_presets()[this->custom_preset];
    }
  } else if (traits.supports_preset(this->preset)) {
    climate->preset = this->preset;
    climate->clear_custom_preset_();
  }
  if (traits.supports_swing_mode(this->swing_mode)) {
    climate->swing_mode = this->swing_mode;
  }
  climate->publish_state();
}

/** Template helper for setting primary modes (fan_mode, preset) with mutual exclusion.
 *
 * Climate devices have mutually exclusive mode pairs:
 *   - fan_mode (enum) vs custom_fan_mode_ (const char*)
 *   - preset (enum) vs custom_preset_ (const char*)
 *
 * Only one mode in each pair can be active at a time. This helper ensures setting a primary
 * mode automatically clears its corresponding custom mode.
 *
 * Example state transitions:
 *   Before: custom_fan_mode_="Turbo", fan_mode=nullopt
 *   Call:   set_fan_mode_(CLIMATE_FAN_HIGH)
 *   After:  custom_fan_mode_=nullptr,   fan_mode=CLIMATE_FAN_HIGH
 *
 * @param primary The primary mode optional (fan_mode or preset)
 * @param custom_ptr Reference to the custom mode pointer (custom_fan_mode_ or custom_preset_)
 * @param value The new primary mode value to set
 * @return true if state changed, false if already set to this value
 */
template<typename T> bool set_primary_mode(optional<T> &primary, const char *&custom_ptr, T value) {
  // Clear the custom mode (mutual exclusion)
  bool changed = custom_ptr != nullptr;
  custom_ptr = nullptr;
  // Set the primary mode
  if (changed || !primary.has_value() || primary.value() != value) {
    primary = value;
    return true;
  }
  return false;
}

/** Template helper for setting custom modes (custom_fan_mode_, custom_preset_) with mutual exclusion.
 *
 * This helper ensures setting a custom mode automatically clears its corresponding primary mode.
 * It also validates that the custom mode exists in the device's supported modes (lifetime safety).
 *
 * Example state transitions:
 *   Before: fan_mode=CLIMATE_FAN_HIGH, custom_fan_mode_=nullptr
 *   Call:   set_custom_fan_mode_("Turbo")
 *   After:  fan_mode=nullopt,          custom_fan_mode_="Turbo" (pointer from traits)
 *
 * Lifetime Safety:
 *   - found_ptr must come from traits.find_custom_*_mode_()
 *   - Only pointers found in traits are stored, ensuring they remain valid
 *   - Prevents dangling pointers from temporary strings
 *
 * @param custom_ptr Reference to the custom mode pointer to set
 * @param primary The primary mode optional to clear
 * @param found_ptr The validated pointer from traits (nullptr if not found)
 * @param has_custom Whether a custom mode is currently active
 * @return true if state changed, false otherwise
 */
template<typename T>
bool set_custom_mode(const char *&custom_ptr, optional<T> &primary, const char *found_ptr, bool has_custom) {
  if (found_ptr != nullptr) {
    // Clear the primary mode (mutual exclusion)
    bool changed = primary.has_value();
    primary.reset();
    // Set the custom mode (pointer is validated by caller from traits)
    if (changed || custom_ptr != found_ptr) {
      custom_ptr = found_ptr;
      return true;
    }
    return false;
  }
  // Mode not found in supported modes, clear it if currently set
  if (has_custom) {
    custom_ptr = nullptr;
    return true;
  }
  return false;
}

bool Climate::set_fan_mode_(ClimateFanMode mode) {
  return set_primary_mode(this->fan_mode, this->custom_fan_mode_, mode);
}

bool Climate::set_custom_fan_mode_(const char *mode) {
  auto traits = this->get_traits();
  return set_custom_mode<ClimateFanMode>(this->custom_fan_mode_, this->fan_mode, traits.find_custom_fan_mode_(mode),
                                         this->has_custom_fan_mode());
}

void Climate::clear_custom_fan_mode_() { this->custom_fan_mode_ = nullptr; }

bool Climate::set_preset_(ClimatePreset preset) { return set_primary_mode(this->preset, this->custom_preset_, preset); }

bool Climate::set_custom_preset_(const char *preset) {
  auto traits = this->get_traits();
  return set_custom_mode<ClimatePreset>(this->custom_preset_, this->preset, traits.find_custom_preset_(preset),
                                        this->has_custom_preset());
}

void Climate::clear_custom_preset_() { this->custom_preset_ = nullptr; }

const char *Climate::find_custom_fan_mode_(const char *custom_fan_mode) {
  return this->get_traits().find_custom_fan_mode_(custom_fan_mode);
}

const char *Climate::find_custom_preset_(const char *custom_preset) {
  return this->get_traits().find_custom_preset_(custom_preset);
}

void Climate::dump_traits_(const char *tag) {
  auto traits = this->get_traits();
  ESP_LOGCONFIG(tag, "ClimateTraits:");
  ESP_LOGCONFIG(tag,
                "  Visual settings:\n"
                "  - Min temperature: %.1f\n"
                "  - Max temperature: %.1f\n"
                "  - Temperature step:\n"
                "      Target: %.1f",
                traits.get_visual_min_temperature(), traits.get_visual_max_temperature(),
                traits.get_visual_target_temperature_step());
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)) {
    ESP_LOGCONFIG(tag, "      Current: %.1f", traits.get_visual_current_temperature_step());
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY |
                               climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)) {
    ESP_LOGCONFIG(tag,
                  "  - Min humidity: %.0f\n"
                  "  - Max humidity: %.0f",
                  traits.get_visual_min_humidity(), traits.get_visual_max_humidity());
  }
  if (traits.has_feature_flags(CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    ESP_LOGCONFIG(tag, "  Supports two-point target temperature");
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)) {
    ESP_LOGCONFIG(tag, "  Supports current temperature");
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY)) {
    ESP_LOGCONFIG(tag, "  Supports target humidity");
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)) {
    ESP_LOGCONFIG(tag, "  Supports current humidity");
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    ESP_LOGCONFIG(tag, "  Supports action");
  }
  if (!traits.get_supported_modes().empty()) {
    ESP_LOGCONFIG(tag, "  Supported modes:");
    for (ClimateMode m : traits.get_supported_modes())
      ESP_LOGCONFIG(tag, "  - %s", LOG_STR_ARG(climate_mode_to_string(m)));
  }
  if (!traits.get_supported_fan_modes().empty()) {
    ESP_LOGCONFIG(tag, "  Supported fan modes:");
    for (ClimateFanMode m : traits.get_supported_fan_modes())
      ESP_LOGCONFIG(tag, "  - %s", LOG_STR_ARG(climate_fan_mode_to_string(m)));
  }
  if (!traits.get_supported_custom_fan_modes().empty()) {
    ESP_LOGCONFIG(tag, "  Supported custom fan modes:");
    for (const char *s : traits.get_supported_custom_fan_modes())
      ESP_LOGCONFIG(tag, "  - %s", s);
  }
  if (!traits.get_supported_presets().empty()) {
    ESP_LOGCONFIG(tag, "  Supported presets:");
    for (ClimatePreset p : traits.get_supported_presets())
      ESP_LOGCONFIG(tag, "  - %s", LOG_STR_ARG(climate_preset_to_string(p)));
  }
  if (!traits.get_supported_custom_presets().empty()) {
    ESP_LOGCONFIG(tag, "  Supported custom presets:");
    for (const char *s : traits.get_supported_custom_presets())
      ESP_LOGCONFIG(tag, "  - %s", s);
  }
  if (!traits.get_supported_swing_modes().empty()) {
    ESP_LOGCONFIG(tag, "  Supported swing modes:");
    for (ClimateSwingMode m : traits.get_supported_swing_modes())
      ESP_LOGCONFIG(tag, "  - %s", LOG_STR_ARG(climate_swing_mode_to_string(m)));
  }
}

}  // namespace esphome::climate
