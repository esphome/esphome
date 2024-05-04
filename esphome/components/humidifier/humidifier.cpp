#include "humidifier.h"
#include "esphome/core/macros.h"

namespace esphome {
namespace humidifier {

static const char *const TAG = "humidifier";

void HumidifierCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->mode_.has_value()) {
    const LogString *mode_s = humidifier_mode_to_string(*this->mode_);
    ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(mode_s));
  }
  if (this->preset_.has_value()) {
    const LogString *preset_s = humidifier_preset_to_string(*this->preset_);
    ESP_LOGD(TAG, "  Preset: %s", LOG_STR_ARG(preset_s));
  }
  if (this->target_humidity_.has_value()) {
    ESP_LOGD(TAG, "  Target Humidity: %.2f", *this->target_humidity_);
  }
  if (this->target_humidity_low_.has_value()) {
    ESP_LOGD(TAG, "  Target Humidity Low: %.2f", *this->target_humidity_low_);
  }
  if (this->target_humidity_high_.has_value()) {
    ESP_LOGD(TAG, "  Target Humidity High: %.2f", *this->target_humidity_high_);
  }
  this->parent_->control(*this);
}
void HumidifierCall::validate_() {
  auto traits = this->parent_->get_traits();
  if (this->mode_.has_value()) {
    auto mode = *this->mode_;
    if (!traits.supports_mode(mode)) {
      ESP_LOGW(TAG, "  Mode %s is not supported by this device!", LOG_STR_ARG(humidifier_mode_to_string(mode)));
      this->mode_.reset();
    }
  }
  if (this->preset_.has_value()) {
    auto preset = *this->preset_;
    if (!traits.supports_preset(preset)) {
      ESP_LOGW(TAG, "  Preset %s is not supported by this device!", LOG_STR_ARG(humidifier_preset_to_string(preset)));
      this->preset_.reset();
    }
  }
  if (this->target_humidity_.has_value()) {
    auto target = *this->target_humidity_;
    if (traits.get_supports_two_point_target_humidity()) {
      ESP_LOGW(TAG, "  Cannot set target humidity for humidifier device "
                    "with two-point target humidity!");
      this->target_humidity_.reset();
    } else if (std::isnan(target)) {
      ESP_LOGW(TAG, "  Target humidity must not be NAN!");
      this->target_humidity_.reset();
    }
  }
  if (this->target_humidity_low_.has_value() || this->target_humidity_high_.has_value()) {
    if (!traits.get_supports_two_point_target_humidity()) {
      ESP_LOGW(TAG, "  Cannot set low/high target humidity for this device!");
      this->target_humidity_low_.reset();
      this->target_humidity_high_.reset();
    }
  }
  if (this->target_humidity_low_.has_value() && std::isnan(*this->target_humidity_low_)) {
    ESP_LOGW(TAG, "  Target humidity low must not be NAN!");
    this->target_humidity_low_.reset();
  }
  if (this->target_humidity_high_.has_value() && std::isnan(*this->target_humidity_high_)) {
    ESP_LOGW(TAG, "  Target humidity low must not be NAN!");
    this->target_humidity_high_.reset();
  }
  if (this->target_humidity_low_.has_value() && this->target_humidity_high_.has_value()) {
    float low = *this->target_humidity_low_;
    float high = *this->target_humidity_high_;
    if (low > high) {
      ESP_LOGW(TAG, "  Target humidity low %.2f must be smaller than target humidity high %.2f!", low, high);
      this->target_humidity_low_.reset();
      this->target_humidity_high_.reset();
    }
  }
}
HumidifierCall &HumidifierCall::set_mode(HumidifierMode mode) {
  this->mode_ = mode;
  return *this;
}
HumidifierCall &HumidifierCall::set_mode(const std::string &mode) {
  if (str_equals_case_insensitive(mode, "OFF")) {
    this->set_mode(HUMIDIFIER_MODE_OFF);
  } else if (str_equals_case_insensitive(mode, "AUTO")) {
    this->set_mode(HUMIDIFIER_MODE_AUTO);
  } else if (str_equals_case_insensitive(mode, "HUMIDIFY")) {
    this->set_mode(HUMIDIFIER_MODE_HUMIDIFY);
  } else if (str_equals_case_insensitive(mode, "DEHUMIDIFY")) {
    this->set_mode(HUMIDIFIER_MODE_DEHUMIDIFY);
  } else if (str_equals_case_insensitive(mode, "HUMIDIFY_DEHUMIDIFY")) {
    this->set_mode(HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  }
  return *this;
}
HumidifierCall &HumidifierCall::set_preset(HumidifierPreset preset) {
  this->preset_ = preset;
  return *this;
}
HumidifierCall &HumidifierCall::set_preset(const std::string &preset) {
  if (str_equals_case_insensitive(preset, "ECO")) {
    this->set_preset(HUMIDIFIER_PRESET_ECO);
  } else if (str_equals_case_insensitive(preset, "AWAY")) {
    this->set_preset(HUMIDIFIER_PRESET_AWAY);
  } else if (str_equals_case_insensitive(preset, "BOOST")) {
    this->set_preset(HUMIDIFIER_PRESET_BOOST);
  } else if (str_equals_case_insensitive(preset, "COMFORT")) {
    this->set_preset(HUMIDIFIER_PRESET_COMFORT);
  } else if (str_equals_case_insensitive(preset, "HOME")) {
    this->set_preset(HUMIDIFIER_PRESET_HOME);
  } else if (str_equals_case_insensitive(preset, "SLEEP")) {
    this->set_preset(HUMIDIFIER_PRESET_SLEEP);
  } else if (str_equals_case_insensitive(preset, "ACTIVITY")) {
    this->set_preset(HUMIDIFIER_PRESET_ACTIVITY);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized preset %s", this->parent_->get_name().c_str(), preset.c_str());
  }
  return *this;
}
HumidifierCall &HumidifierCall::set_preset(optional<std::string> preset) {
  if (preset.has_value()) {
    this->set_preset(preset.value());
  }
  return *this;
}

HumidifierCall &HumidifierCall::set_target_humidity(float target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}
HumidifierCall &HumidifierCall::set_target_humidity_low(float target_humidity_low) {
  this->target_humidity_low_ = target_humidity_low;
  return *this;
}
HumidifierCall &HumidifierCall::set_target_humidity_high(float target_humidity_high) {
  this->target_humidity_high_ = target_humidity_high;
  return *this;
}
const optional<HumidifierMode> &HumidifierCall::get_mode() const { return this->mode_; }
const optional<float> &HumidifierCall::get_target_humidity() const { return this->target_humidity_; }
const optional<float> &HumidifierCall::get_target_humidity_low() const { return this->target_humidity_low_; }
const optional<float> &HumidifierCall::get_target_humidity_high() const { return this->target_humidity_high_; }
const optional<HumidifierPreset> &HumidifierCall::get_preset() const { return this->preset_; }
HumidifierCall &HumidifierCall::set_target_humidity_high(optional<float> target_humidity_high) {
  this->target_humidity_high_ = target_humidity_high;
  return *this;
}
HumidifierCall &HumidifierCall::set_target_humidity_low(optional<float> target_humidity_low) {
  this->target_humidity_low_ = target_humidity_low;
  return *this;
}
HumidifierCall &HumidifierCall::set_target_humidity(optional<float> target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}
HumidifierCall &HumidifierCall::set_mode(optional<HumidifierMode> mode) {
  this->mode_ = mode;
  return *this;
}
HumidifierCall &HumidifierCall::set_preset(optional<HumidifierPreset> preset) {
  this->preset_ = preset;
  return *this;
}

void Humidifier::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}

// Random 32bit value; If this changes existing restore preferences are invalidated
static const uint32_t RESTORE_STATE_VERSION = 0x848EA6ADUL;

optional<HumidifierDeviceRestoreState> Humidifier::restore_state_() {
  this->rtc_ = global_preferences->make_preference<HumidifierDeviceRestoreState>(this->get_object_id_hash() ^
                                                                                 RESTORE_STATE_VERSION);
  HumidifierDeviceRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}
void Humidifier::save_state_() {
#if (defined(USE_ESP_IDF) || (defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 0, 0))) && \
    !defined(CLANG_TIDY)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#define TEMP_IGNORE_MEMACCESS
#endif
  HumidifierDeviceRestoreState state{};
  // initialize as zero to prevent random data on stack triggering erase
  memset(&state, 0, sizeof(HumidifierDeviceRestoreState));
#ifdef TEMP_IGNORE_MEMACCESS
#pragma GCC diagnostic pop
#undef TEMP_IGNORE_MEMACCESS
#endif

  state.mode = this->mode;
  auto traits = this->get_traits();
  if (traits.get_supports_two_point_target_humidity()) {
    state.target_humidity_low = this->target_humidity_low;
    state.target_humidity_high = this->target_humidity_high;
  } else {
    state.target_humidity = this->target_humidity;
  }
  if (traits.get_supports_presets() && preset.has_value()) {
    state.preset = this->preset.value();
  }

  this->rtc_.save(&state);
}
void Humidifier::publish_state() {
  ESP_LOGD(TAG, "'%s' - Sending state:", this->name_.c_str());
  auto traits = this->get_traits();

  ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(humidifier_mode_to_string(this->mode)));
  if (traits.get_supports_action()) {
    ESP_LOGD(TAG, "  Action: %s", LOG_STR_ARG(humidifier_action_to_string(this->action)));
  }
  if (traits.get_supports_presets() && this->preset.has_value()) {
    ESP_LOGD(TAG, "  Preset: %s", LOG_STR_ARG(humidifier_preset_to_string(this->preset.value())));
  }
  if (traits.get_supports_current_humidity()) {
    ESP_LOGD(TAG, "  Current Humidity: %.2f%%", this->current_humidity);
  }
  if (traits.get_supports_two_point_target_humidity()) {
    ESP_LOGD(TAG, "  Target Humidity: Low: %.2f%% High: %.2f%%", this->target_humidity_low, this->target_humidity_high);
  } else {
    ESP_LOGD(TAG, "  Target Humidity: %.2f%%", this->target_humidity);
  }

  // Send state to frontend
  this->state_callback_.call();
  // Save state
  this->save_state_();
}
uint32_t Humidifier::hash_base() { return 3104134496UL; }

HumidifierTraits Humidifier::get_traits() {
  auto traits = this->traits();
  if (this->visual_min_humidity_override_.has_value()) {
    traits.set_visual_min_humidity(*this->visual_min_humidity_override_);
  }
  if (this->visual_max_humidity_override_.has_value()) {
    traits.set_visual_max_humidity(*this->visual_max_humidity_override_);
  }
  if (this->visual_humidity_step_override_.has_value()) {
    traits.set_visual_humidity_step(*this->visual_humidity_step_override_);
  }
  return traits;
}

void Humidifier::set_visual_min_humidity_override(float visual_min_humidity_override) {
  this->visual_min_humidity_override_ = visual_min_humidity_override;
}
void Humidifier::set_visual_max_humidity_override(float visual_max_humidity_override) {
  this->visual_max_humidity_override_ = visual_max_humidity_override;
}
void Humidifier::set_visual_humidity_step_override(float visual_humidity_step_override) {
  this->visual_humidity_step_override_ = visual_humidity_step_override;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
Humidifier::Humidifier(const std::string &name) : EntityBase(name) {}
#pragma GCC diagnostic pop

Humidifier::Humidifier() : Humidifier("") {}
HumidifierCall Humidifier::make_call() { return HumidifierCall(this); }

HumidifierCall HumidifierDeviceRestoreState::to_call(Humidifier *humidifier) {
  auto call = humidifier->make_call();
  auto traits = humidifier->get_traits();
  call.set_mode(this->mode);
  if (traits.get_supports_two_point_target_humidity()) {
    call.set_target_humidity_low(this->target_humidity_low);
    call.set_target_humidity_high(this->target_humidity_high);
  } else {
    call.set_target_humidity(this->target_humidity);
  }
  if (traits.get_supports_presets()) {
    call.set_preset(this->preset);
  }
  return call;
}
void HumidifierDeviceRestoreState::apply(Humidifier *humidifier) {
  auto traits = humidifier->get_traits();
  humidifier->mode = this->mode;
  if (traits.get_supports_two_point_target_humidity()) {
    humidifier->target_humidity_low = this->target_humidity_low;
    humidifier->target_humidity_high = this->target_humidity_high;
  } else {
    humidifier->target_humidity = this->target_humidity;
  }
  if (traits.get_supports_presets()) {
    humidifier->preset = this->preset;
  }
  humidifier->publish_state();
}

template<typename T1> bool set_alternative(optional<T1> &dst, const T1 &src) {
  bool is_changed = false;
  if (dst != src) {
    dst = src;
    is_changed = true;
  }
  return is_changed;
}

bool Humidifier::set_preset_(HumidifierPreset preset) { return set_alternative(this->preset, preset); }

void Humidifier::dump_traits_(const char *tag) {
  auto traits = this->get_traits();
  ESP_LOGCONFIG(tag, "HumidifierTraits:");
  ESP_LOGCONFIG(tag, "  [x] Visual settings:");
  ESP_LOGCONFIG(tag, "      - Min: %.1f", traits.get_visual_min_humidity());
  ESP_LOGCONFIG(tag, "      - Max: %.1f", traits.get_visual_max_humidity());
  ESP_LOGCONFIG(tag, "      - Step: %.1f", traits.get_visual_humidity_step());
  if (traits.get_supports_current_humidity())
    ESP_LOGCONFIG(tag, "  [x] Supports current humidity");
  if (traits.get_supports_two_point_target_humidity())
    ESP_LOGCONFIG(tag, "  [x] Supports two-point target humidity");
  if (traits.get_supports_action())
    ESP_LOGCONFIG(tag, "  [x] Supports action");
  if (!traits.get_supported_modes().empty()) {
    ESP_LOGCONFIG(tag, "  [x] Supported modes:");
    for (HumidifierMode m : traits.get_supported_modes())
      ESP_LOGCONFIG(tag, "      - %s", LOG_STR_ARG(humidifier_mode_to_string(m)));
  }
  if (!traits.get_supported_presets().empty()) {
    ESP_LOGCONFIG(tag, "  [x] Supported presets:");
    for (HumidifierPreset p : traits.get_supported_presets())
      ESP_LOGCONFIG(tag, "      - %s", LOG_STR_ARG(humidifier_preset_to_string(p)));
  }
}

}  // namespace humidifier
}  // namespace esphome
