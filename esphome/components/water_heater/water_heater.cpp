#include "water_heater.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/controller_registry.h"

#include <cmath>

namespace esphome::water_heater {

static const char *const TAG = "water_heater";

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

void WaterHeaterCall::apply(WaterHeater *water_heater) { *this = water_heater->make_call(); }

WaterHeaterCall &WaterHeaterCall::to_call(WaterHeater *water_heater) {
  water_heater->make_call().set_from_restore(*this).perform();
  return *this;
}

void WaterHeaterCall::perform() {
  this->validate_();
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
    if (this->target_temperature_ < traits.get_min_temperature() ||
        this->target_temperature_ > traits.get_max_temperature()) {
      ESP_LOGW(TAG, "'%s' - Target temperature %.1f is out of range [%.1f - %.1f]", this->parent_->get_name().c_str(),
               this->target_temperature_, traits.get_min_temperature(), traits.get_max_temperature());

      if (this->target_temperature_ < traits.get_min_temperature())
        this->target_temperature_ = traits.get_min_temperature();
      if (this->target_temperature_ > traits.get_max_temperature())
        this->target_temperature_ = traits.get_max_temperature();
    }
  }
}

void WaterHeaterTraits::set_supports_current_temperature(bool supports_current_temperature) {
  this->supports_current_temperature_ = supports_current_temperature;
}
bool WaterHeaterTraits::get_supports_current_temperature() const { return this->supports_current_temperature_; }

void WaterHeaterTraits::set_min_temperature(float min_temperature) { this->min_temperature_ = min_temperature; }
float WaterHeaterTraits::get_min_temperature() const { return this->min_temperature_; }

void WaterHeaterTraits::set_max_temperature(float max_temperature) { this->max_temperature_ = max_temperature; }
float WaterHeaterTraits::get_max_temperature() const { return this->max_temperature_; }

void WaterHeater::setup() {
  this->pref_ = global_preferences->make_preference<SavedWaterHeaterState>(this->get_object_id_hash());
}

void WaterHeater::publish_state() {
#if defined(USE_WATER_HEATER) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_water_heater_update(this);
#endif

  SavedWaterHeaterState saved{};
  saved.mode = this->mode;
  saved.target_temperature = this->target_temperature;
  this->pref_.save(&saved);
}

optional<WaterHeaterCall> WaterHeater::restore_state() {
  SavedWaterHeaterState recovered{};
  if (!this->pref_.load(&recovered))
    return {};

  auto call = this->make_call();
  call.set_mode(recovered.mode);
  call.set_target_temperature(recovered.target_temperature);
  return call;
}

WaterHeaterTraits WaterHeater::get_traits() {
  auto traits = this->traits();
  if (this->visual_min_temperature_override_.has_value()) {
    traits.set_min_temperature(*this->visual_min_temperature_override_);
  }
  if (this->visual_max_temperature_override_.has_value()) {
    traits.set_max_temperature(*this->visual_max_temperature_override_);
  }
  return traits;
}

void WaterHeater::set_visual_min_temperature_override(float min_temperature_override) {
  this->visual_min_temperature_override_ = min_temperature_override;
}
void WaterHeater::set_visual_max_temperature_override(float max_temperature_override) {
  this->visual_max_temperature_override_ = max_temperature_override;
}

}  // namespace esphome::water_heater
