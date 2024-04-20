#include "humidifier.h"
#include "esphome/core/macros.h"

namespace esphome {
namespace humidifier {

static const char *const TAG = "humidifier";

void HumidifierCall::perform() {
  this->parent_->control_callback_.call(*this);
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->mode_.has_value()) {
    const LogString *mode_s = humidifier_mode_to_string(*this->mode_);
    ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(mode_s));
  }
  if (this->target_humidity_.has_value()) {
    ESP_LOGD(TAG, "  Target Humidity: %.0f", *this->target_humidity_);
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
  if (this->target_humidity_.has_value()) {
    auto target = *this->target_humidity_;
    if (traits.get_supports_target_humidity()) {
      ESP_LOGW(TAG, "  Target humidity must not be NAN!");
      this->target_humidity_.reset();
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
  } else if (str_equals_case_insensitive(mode, "NORMAL")) {
    this->set_mode(HUMIDIFIER_MODE_NORMAL);
  } else if (str_equals_case_insensitive(mode, "ECO")) {
    this->set_mode(HUMIDIFIER_MODE_ECO);
  } else if (str_equals_case_insensitive(mode, "AWAY")) {
    this->set_mode(HUMIDIFIER_MODE_AWAY);
  } else if (str_equals_case_insensitive(mode, "BOOST")) {
    this->set_mode(HUMIDIFIER_MODE_BOOST);
  } else if (str_equals_case_insensitive(mode, "COMFORT")) {
    this->set_mode(HUMIDIFIER_MODE_COMFORT);
  } else if (str_equals_case_insensitive(mode, "HOME")) {
    this->set_mode(HUMIDIFIER_MODE_HOME);
  } else if (str_equals_case_insensitive(mode, "SLEEP")) {
    this->set_mode(HUMIDIFIER_MODE_SLEEP);
  } else if (str_equals_case_insensitive(mode, "AUTO")) {
    this->set_mode(HUMIDIFIER_MODE_AUTO);
  } else if (str_equals_case_insensitive(mode, "BABY")) {
    this->set_mode(HUMIDIFIER_MODE_BABY);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  }
  return *this;
}

HumidifierCall &HumidifierCall::set_target_humidity(float target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}

const optional<HumidifierMode> &HumidifierCall::get_mode() const { return this->mode_; }
const optional<float> &HumidifierCall::get_target_humidity() const { return this->target_humidity_; }

HumidifierCall &HumidifierCall::set_target_humidity(optional<float> target_humidity) {
  this->target_humidity_ = target_humidity;
  return *this;
}
HumidifierCall &HumidifierCall::set_mode(optional<HumidifierMode> mode) {
  this->mode_ = mode;
  return *this;
}

void Humidifier::add_on_state_callback(std::function<void(Humidifier &)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void Humidifier::add_on_control_callback(std::function<void(HumidifierCall &)> &&callback) {
  this->control_callback_.add(std::move(callback));
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
  if (traits.get_supports_target_humidity()) {
    state.target_humidity = this->target_humidity;
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
  if (traits.get_supports_current_humidity()) {
    ESP_LOGD(TAG, "  Current Humidity: %.0f%%", this->current_humidity);
  }
  if (traits.get_supports_target_humidity()) {
    ESP_LOGD(TAG, "  Target Humidity: %.0f%%", this->target_humidity);
  }
  // Send state to frontend
  this->state_callback_.call(*this);
  // Save state
  this->save_state_();
}
HumidifierTraits Humidifier::get_traits() {
  auto traits = this->traits();
  if (this->visual_min_humidity_override_.has_value()) {
    traits.set_visual_min_humidity(*this->visual_min_humidity_override_);
  }
  if (this->visual_max_humidity_override_.has_value()) {
    traits.set_visual_max_humidity(*this->visual_max_humidity_override_);
  }
  if (this->visual_target_humidity_step_override_.has_value()) {
    traits.set_visual_target_humidity_step(*this->visual_target_humidity_step_override_);
    traits.set_visual_current_humidity_step(*this->visual_current_humidity_step_override_);
  }

  return traits;
}

void Humidifier::set_visual_min_humidity_override(float visual_min_humidity_override) {
  this->visual_min_humidity_override_ = visual_min_humidity_override;
}
void Humidifier::set_visual_max_humidity_override(float visual_max_humidity_override) {
  this->visual_max_humidity_override_ = visual_max_humidity_override;
}
void Humidifier::set_visual_humidity_step_override(float target, float current) {
  this->visual_target_humidity_step_override_ = target;
  this->visual_current_humidity_step_override_ = current;
}

HumidifierCall Humidifier::make_call() { return HumidifierCall(this); }

HumidifierCall HumidifierDeviceRestoreState::to_call(Humidifier *humidifier) {
  auto call = humidifier->make_call();
  auto traits = humidifier->get_traits();
  call.set_mode(this->mode);
  if (traits.get_supports_target_humidity()) {
    call.set_target_humidity(this->target_humidity);
  }
  return call;
}

void HumidifierDeviceRestoreState::apply(Humidifier *humidifier) {
  auto traits = humidifier->get_traits();
  humidifier->mode = this->mode;
  if (traits.get_supports_target_humidity()) {
    humidifier->target_humidity = this->target_humidity;
  }
  humidifier->publish_state();
}

template<typename T1, typename T2> bool set_alternative(optional<T1> &dst, optional<T2> &alt, const T1 &src) {
  bool is_changed = alt.has_value();
  alt.reset();
  if (is_changed || dst != src) {
    dst = src;
    is_changed = true;
  }
  return is_changed;
}

void Humidifier::dump_traits_(const char *tag) {
  auto traits = this->get_traits();
  ESP_LOGCONFIG(tag, "HumidifierTraits:");
  ESP_LOGCONFIG(tag, "  [x] Visual settings:");
  ESP_LOGCONFIG(tag, "      - Min humidity: %.1f", traits.get_visual_min_humidity());
  ESP_LOGCONFIG(tag, "      - Max humidity: %.1f", traits.get_visual_max_humidity());
  ESP_LOGCONFIG(tag, "      - Humidity step:");
  ESP_LOGCONFIG(tag, "          Target: %.0f%%", traits.get_visual_target_humidity_step());
  ESP_LOGCONFIG(tag, "          Current: %.1F%%", traits.get_visual_current_humidity_step());
  if (traits.get_supports_current_humidity()) {
    ESP_LOGCONFIG(tag, "  [x] Supports current humidity");
  }
  if (traits.get_supports_action()) {
    ESP_LOGCONFIG(tag, "  [x] Supports action");
  }
  if (!traits.get_supported_modes().empty()) {
    ESP_LOGCONFIG(tag, "  [x] Supported modes:");
    for (HumidifierMode m : traits.get_supported_modes())
      ESP_LOGCONFIG(tag, "      - %s", LOG_STR_ARG(humidifier_mode_to_string(m)));
  }
}

}  // namespace humidifier
}  // namespace esphome
