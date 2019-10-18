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
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
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

  this->rtc_.save(&state);
}
void Climate::publish_state() {
  ESP_LOGD(TAG, "'%s' - Sending state:", this->name_.c_str());
  auto traits = this->get_traits();

  ESP_LOGD(TAG, "  Mode: %s", climate_mode_to_string(this->mode));
  if (traits.get_supports_action()) {
    ESP_LOGD(TAG, "  Action: %s", climate_action_to_string(this->action));
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
  climate->publish_state();
}

}  // namespace climate
}  // namespace esphome
