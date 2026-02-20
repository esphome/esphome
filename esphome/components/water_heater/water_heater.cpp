#include "water_heater.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/controller_registry.h"

#include <cmath>

namespace esphome::water_heater {

static const char *const TAG = "water_heater";

void log_water_heater(const char *tag, const char *prefix, const char *type, WaterHeater *obj) {
  if (obj != nullptr) {
    ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());
  }
}

WaterHeaterCall::WaterHeaterCall(WaterHeater *parent) : parent_(parent) {}

WaterHeaterCall &WaterHeaterCall::set_mode(WaterHeaterMode mode) {
  this->mode_ = mode;
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_mode(const std::string &mode) {
  if (str_equals_case_insensitive(mode, "OFF")) {
    this->set_mode(WATER_HEATER_MODE_OFF);
  } else if (str_equals_case_insensitive(mode, "ECO")) {
    this->set_mode(WATER_HEATER_MODE_ECO);
  } else if (str_equals_case_insensitive(mode, "ELECTRIC")) {
    this->set_mode(WATER_HEATER_MODE_ELECTRIC);
  } else if (str_equals_case_insensitive(mode, "PERFORMANCE")) {
    this->set_mode(WATER_HEATER_MODE_PERFORMANCE);
  } else if (str_equals_case_insensitive(mode, "HIGH_DEMAND")) {
    this->set_mode(WATER_HEATER_MODE_HIGH_DEMAND);
  } else if (str_equals_case_insensitive(mode, "HEAT_PUMP")) {
    this->set_mode(WATER_HEATER_MODE_HEAT_PUMP);
  } else if (str_equals_case_insensitive(mode, "GAS")) {
    this->set_mode(WATER_HEATER_MODE_GAS);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mode %s", this->parent_->get_name().c_str(), mode.c_str());
  }
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_target_temperature(float temperature) {
  this->target_temperature_ = temperature;
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_target_temperature_low(float temperature) {
  this->target_temperature_low_ = temperature;
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_target_temperature_high(float temperature) {
  this->target_temperature_high_ = temperature;
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_away(bool away) {
  if (away) {
    this->state_ |= WATER_HEATER_STATE_AWAY;
  } else {
    this->state_ &= ~WATER_HEATER_STATE_AWAY;
  }
  return *this;
}

WaterHeaterCall &WaterHeaterCall::set_on(bool on) {
  if (on) {
    this->state_ |= WATER_HEATER_STATE_ON;
  } else {
    this->state_ &= ~WATER_HEATER_STATE_ON;
  }
  return *this;
}

void WaterHeaterCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->mode_.has_value()) {
    ESP_LOGD(TAG, "  Mode: %s", LOG_STR_ARG(water_heater_mode_to_string(*this->mode_)));
  }
  if (!std::isnan(this->target_temperature_)) {
    ESP_LOGD(TAG, "  Target Temperature: %.2f", this->target_temperature_);
  }
  if (!std::isnan(this->target_temperature_low_)) {
    ESP_LOGD(TAG, "  Target Temperature Low: %.2f", this->target_temperature_low_);
  }
  if (!std::isnan(this->target_temperature_high_)) {
    ESP_LOGD(TAG, "  Target Temperature High: %.2f", this->target_temperature_high_);
  }
  if (this->state_ & WATER_HEATER_STATE_AWAY) {
    ESP_LOGD(TAG, "  Away: YES");
  }
  if (this->state_ & WATER_HEATER_STATE_ON) {
    ESP_LOGD(TAG, "  On: YES");
  }
  this->parent_->control(*this);
}

void WaterHeaterCall::validate_() {
  auto traits = this->parent_->get_traits();
  if (this->mode_.has_value()) {
    if (!traits.supports_mode(*this->mode_)) {
      ESP_LOGW(TAG, "'%s' - Mode %d not supported", this->parent_->get_name().c_str(), *this->mode_);
      this->mode_.reset();
    }
  }
  if (!std::isnan(this->target_temperature_)) {
    if (traits.get_supports_two_point_target_temperature()) {
      ESP_LOGW(TAG, "'%s' - Cannot set target temperature for device with two-point target temperature",
               this->parent_->get_name().c_str());
      this->target_temperature_ = NAN;
    } else if (this->target_temperature_ < traits.get_min_temperature() ||
               this->target_temperature_ > traits.get_max_temperature()) {
      ESP_LOGW(TAG, "'%s' - Target temperature %.1f is out of range [%.1f - %.1f]", this->parent_->get_name().c_str(),
               this->target_temperature_, traits.get_min_temperature(), traits.get_max_temperature());
      this->target_temperature_ =
          std::max(traits.get_min_temperature(), std::min(this->target_temperature_, traits.get_max_temperature()));
    }
  }
  if (!std::isnan(this->target_temperature_low_) || !std::isnan(this->target_temperature_high_)) {
    if (!traits.get_supports_two_point_target_temperature()) {
      ESP_LOGW(TAG, "'%s' - Cannot set low/high target temperature", this->parent_->get_name().c_str());
      this->target_temperature_low_ = NAN;
      this->target_temperature_high_ = NAN;
    }
  }
  if (!std::isnan(this->target_temperature_low_) && !std::isnan(this->target_temperature_high_)) {
    if (this->target_temperature_low_ > this->target_temperature_high_) {
      ESP_LOGW(TAG, "'%s' - Target temperature low %.2f must be less than high %.2f", this->parent_->get_name().c_str(),
               this->target_temperature_low_, this->target_temperature_high_);
      this->target_temperature_low_ = NAN;
      this->target_temperature_high_ = NAN;
    }
  }
  if ((this->state_ & WATER_HEATER_STATE_AWAY) && !traits.get_supports_away_mode()) {
    ESP_LOGW(TAG, "'%s' - Away mode not supported", this->parent_->get_name().c_str());
    this->state_ &= ~WATER_HEATER_STATE_AWAY;
  }
  // If ON/OFF not supported, device is always on - clear the flag silently
  if (!traits.has_feature_flags(WATER_HEATER_SUPPORTS_ON_OFF)) {
    this->state_ &= ~WATER_HEATER_STATE_ON;
  }
}

void WaterHeater::setup() {
  this->pref_ = global_preferences->make_preference<SavedWaterHeaterState>(this->get_preference_hash());
}

void WaterHeater::publish_state() {
  auto traits = this->get_traits();
  ESP_LOGD(TAG,
           "'%s' >>\n"
           "  Mode: %s",
           this->name_.c_str(), LOG_STR_ARG(water_heater_mode_to_string(this->mode_)));
  if (!std::isnan(this->current_temperature_)) {
    ESP_LOGD(TAG, "  Current Temperature: %.2f°C", this->current_temperature_);
  }
  if (traits.get_supports_two_point_target_temperature()) {
    ESP_LOGD(TAG, "  Target Temperature: Low: %.2f°C High: %.2f°C", this->target_temperature_low_,
             this->target_temperature_high_);
  } else if (!std::isnan(this->target_temperature_)) {
    ESP_LOGD(TAG, "  Target Temperature: %.2f°C", this->target_temperature_);
  }
  if (this->state_ & WATER_HEATER_STATE_AWAY) {
    ESP_LOGD(TAG, "  Away: YES");
  }
  if (traits.has_feature_flags(WATER_HEATER_SUPPORTS_ON_OFF)) {
    ESP_LOGD(TAG, "  On: %s", (this->state_ & WATER_HEATER_STATE_ON) ? "YES" : "NO");
  }

#if defined(USE_WATER_HEATER) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_water_heater_update(this);
#endif

  SavedWaterHeaterState saved{};
  saved.mode = this->mode_;
  if (traits.get_supports_two_point_target_temperature()) {
    saved.target_temperature_low = this->target_temperature_low_;
    saved.target_temperature_high = this->target_temperature_high_;
  } else {
    saved.target_temperature = this->target_temperature_;
  }
  saved.state = this->state_;
  this->pref_.save(&saved);
}

optional<WaterHeaterCall> WaterHeater::restore_state() {
  SavedWaterHeaterState recovered{};
  if (!this->pref_.load(&recovered))
    return {};

  auto traits = this->get_traits();
  auto call = this->make_call();
  call.set_mode(recovered.mode);
  if (traits.get_supports_two_point_target_temperature()) {
    call.set_target_temperature_low(recovered.target_temperature_low);
    call.set_target_temperature_high(recovered.target_temperature_high);
  } else {
    call.set_target_temperature(recovered.target_temperature);
  }
  call.set_away((recovered.state & WATER_HEATER_STATE_AWAY) != 0);
  call.set_on((recovered.state & WATER_HEATER_STATE_ON) != 0);
  return call;
}

WaterHeaterTraits WaterHeater::get_traits() {
  auto traits = this->traits();
#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
  if (!std::isnan(this->visual_min_temperature_override_)) {
    traits.set_min_temperature(this->visual_min_temperature_override_);
  }
  if (!std::isnan(this->visual_max_temperature_override_)) {
    traits.set_max_temperature(this->visual_max_temperature_override_);
  }
  if (!std::isnan(this->visual_target_temperature_step_override_)) {
    traits.set_target_temperature_step(this->visual_target_temperature_step_override_);
  }
#endif
  return traits;
}

#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
void WaterHeater::set_visual_min_temperature_override(float min_temperature_override) {
  this->visual_min_temperature_override_ = min_temperature_override;
}
void WaterHeater::set_visual_max_temperature_override(float max_temperature_override) {
  this->visual_max_temperature_override_ = max_temperature_override;
}
void WaterHeater::set_visual_target_temperature_step_override(float visual_target_temperature_step_override) {
  this->visual_target_temperature_step_override_ = visual_target_temperature_step_override;
}
#endif

const LogString *water_heater_mode_to_string(WaterHeaterMode mode) {
  switch (mode) {
    case WATER_HEATER_MODE_OFF:
      return LOG_STR("OFF");
    case WATER_HEATER_MODE_ECO:
      return LOG_STR("ECO");
    case WATER_HEATER_MODE_ELECTRIC:
      return LOG_STR("ELECTRIC");
    case WATER_HEATER_MODE_PERFORMANCE:
      return LOG_STR("PERFORMANCE");
    case WATER_HEATER_MODE_HIGH_DEMAND:
      return LOG_STR("HIGH_DEMAND");
    case WATER_HEATER_MODE_HEAT_PUMP:
      return LOG_STR("HEAT_PUMP");
    case WATER_HEATER_MODE_GAS:
      return LOG_STR("GAS");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void WaterHeater::dump_traits_(const char *tag) {
  auto traits = this->get_traits();
  ESP_LOGCONFIG(tag,
                "  Min Temperature: %.1f°C\n"
                "  Max Temperature: %.1f°C\n"
                "  Temperature Step: %.1f",
                traits.get_min_temperature(), traits.get_max_temperature(), traits.get_target_temperature_step());
  if (traits.get_supports_two_point_target_temperature()) {
    ESP_LOGCONFIG(tag, "  Supports Two-Point Target Temperature: YES");
  }
  if (traits.get_supports_away_mode()) {
    ESP_LOGCONFIG(tag, "  Supports Away Mode: YES");
  }
  if (traits.has_feature_flags(WATER_HEATER_SUPPORTS_ON_OFF)) {
    ESP_LOGCONFIG(tag, "  Supports On/Off: YES");
  }
  if (!traits.get_supported_modes().empty()) {
    ESP_LOGCONFIG(tag, "  Supported Modes:");
    for (WaterHeaterMode m : traits.get_supported_modes()) {
      ESP_LOGCONFIG(tag, "    - %s", LOG_STR_ARG(water_heater_mode_to_string(m)));
    }
  }
}

}  // namespace esphome::water_heater
