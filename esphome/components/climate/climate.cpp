#include "climate.h"
#include "esphome/core/log.h"

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
  if (this->fan_mode_.has_value()) {
    const char *fan_mode_s = climate_fan_mode_to_string(*this->fan_mode_);
    ESP_LOGD(TAG, "  Fan: %s", fan_mode_s);
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
  if (this->boost_.has_value()) {
    ESP_LOGD(TAG, "  Boost Mode: %s", ONOFF(*this->boost_));
  }
  if (this->eco_.has_value()) {
    ESP_LOGD(TAG, "  Eco Mode: %s", ONOFF(*this->eco_));
  }
  if (this->sleep_.has_value()) {
    ESP_LOGD(TAG, "  sleep Mode: %s", ONOFF(*this->sleep_));
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
  if (this->fan_mode_.has_value()) {
    auto fan_mode = *this->fan_mode_;
    if (!traits.supports_fan_mode(fan_mode)) {
      ESP_LOGW(TAG, "  Fan Mode %s is not supported by this device!", climate_fan_mode_to_string(fan_mode));
      this->fan_mode_.reset();
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
  if (this->boost_.has_value()) {
    if (!traits.get_supports_boost()) {
      ESP_LOGW(TAG, "  Cannot set boost mode for this device!");
      this->boost_.reset();
    }
  }
  if (this->eco_.has_value()) {
    if (!traits.get_supports_eco()) {
      ESP_LOGW(TAG, "  Cannot set eco mode for this device!");
      this->eco_.reset();
    }
  }
  if (this->sleep_.has_value()) {
    if (!traits.get_supports_sleep()) {
      ESP_LOGW(TAG, "  Cannot set sleep mode for this device!");
      this->sleep_.reset();
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
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  }
  return *this;
}
ClimateCall &ClimateCall::set_fan_mode(ClimateFanMode fan_mode) {
  this->fan_mode_ = fan_mode;
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
    ESP_LOGW(TAG, "'%s' - Unrecognized fan mode %s", this->parent_->get_name().c_str(), fan_mode.c_str());
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
const optional<bool> &ClimateCall::get_boost() const { return this->boost_; }
const optional<bool> &ClimateCall::get_eco() const { return this->eco_; }
const optional<bool> &ClimateCall::get_sleep() const { return this->sleep_; }
const optional<ClimateFanMode> &ClimateCall::get_fan_mode() const { return this->fan_mode_; }
const optional<ClimateSwingMode> &ClimateCall::get_swing_mode() const { return this->swing_mode_; }
ClimateCall &ClimateCall::set_away(bool away) {
  this->away_ = away;
  return *this;
}
ClimateCall &ClimateCall::set_away(optional<bool> away) {
  this->away_ = away;
  return *this;
}
ClimateCall &ClimateCall::set_boost(bool boost) {
  this->boost_ = boost;
  return *this;
}
ClimateCall &ClimateCall::set_boost(optional<bool> boost) {
  this->boost_ = boost;
  return *this;
}
ClimateCall &ClimateCall::set_eco(bool eco) {
  this->eco_ = eco;
  return *this;
}
ClimateCall &ClimateCall::set_eco(optional<bool> eco) {
  this->eco_ = eco;
  return *this;
}
ClimateCall &ClimateCall::set_sleep(bool sleep) {
  this->sleep_ = sleep;
  return *this;
}
ClimateCall &ClimateCall::set_sleep(optional<bool> sleep) {
  this->sleep_ = sleep;
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
  if (traits.get_supports_boost()) {
    state.boost = this->boost;
  }
  if (traits.get_supports_eco()) {
    state.eco = this->eco;
  }
  if (traits.get_supports_sleep()) {
    state.sleep = this->sleep;
  }
  if (traits.get_supports_fan_modes()) {
    state.fan_mode = this->fan_mode;
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
  if (traits.get_supports_fan_modes()) {
    ESP_LOGD(TAG, "  Fan Mode: %s", climate_fan_mode_to_string(this->fan_mode));
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
  if (traits.get_supports_boost()) {
    ESP_LOGD(TAG, "  Boost: %s", ONOFF(this->boost));
  }
  if (traits.get_supports_eco()) {
    ESP_LOGD(TAG, "  Eco: %s", ONOFF(this->eco));
  }
  if (traits.get_supports_sleep()) {
    ESP_LOGD(TAG, "  sleep: %s", ONOFF(this->sleep));
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
  if (traits.get_supports_boost()) {
    call.set_boost(this->boost);
  }
  if (traits.get_supports_eco()) {
    call.set_eco(this->eco);
  }
  if (traits.get_supports_sleep()) {
    call.set_sleep(this->sleep);
  }
  if (traits.get_supports_fan_modes()) {
    call.set_fan_mode(this->fan_mode);
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
  if (traits.get_supports_boost()) {
    climate->boost = this->boost;
  }
  if (traits.get_supports_eco()) {
    climate->eco = this->eco;
  }
  if (traits.get_supports_sleep()) {
    climate->sleep = this->sleep;
  }
  if (traits.get_supports_fan_modes()) {
    climate->fan_mode = this->fan_mode;
  }
  if (traits.get_supports_swing_modes()) {
    climate->swing_mode = this->swing_mode;
  }
  climate->publish_state();
}

}  // namespace climate
}  // namespace esphome
