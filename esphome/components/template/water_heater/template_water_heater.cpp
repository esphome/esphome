#include "template_water_heater.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.water_heater";

TemplateWaterHeater::TemplateWaterHeater() : set_trigger_(new Trigger<>()) {}

void TemplateWaterHeater::setup() {
  if (this->restore_mode_ == TemplateWaterHeaterRestoreMode::WATER_HEATER_RESTORE ||
      this->restore_mode_ == TemplateWaterHeaterRestoreMode::WATER_HEATER_RESTORE_AND_CALL) {
    auto restore = this->restore_state();

    if (restore.has_value()) {
      restore->perform();
    }
  }
  if (!this->current_temperature_f_.has_value() && !this->mode_f_.has_value())
    this->disable_loop();
}

water_heater::WaterHeaterTraits TemplateWaterHeater::traits() {
  water_heater::WaterHeaterTraits traits;

  if (!this->supported_modes_.empty()) {
    traits.set_supported_modes(this->supported_modes_);
  }

  traits.set_supports_current_temperature(true);
  return traits;
}

void TemplateWaterHeater::loop() {
  bool changed = false;

  auto curr_temp = this->current_temperature_f_.call();
  if (curr_temp.has_value()) {
    if (*curr_temp != this->current_temperature_) {
      this->current_temperature_ = *curr_temp;
      changed = true;
    }
  }

  auto new_mode = this->mode_f_.call();
  if (new_mode.has_value()) {
    if (*new_mode != this->mode_) {
      this->mode_ = *new_mode;
      changed = true;
    }
  }

  if (changed) {
    this->publish_state();
  }
}

void TemplateWaterHeater::dump_config() {
  LOG_WATER_HEATER("", "Template Water Heater", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

float TemplateWaterHeater::get_setup_priority() const { return setup_priority::HARDWARE; }

water_heater::WaterHeaterCallInternal TemplateWaterHeater::make_call() {
  return water_heater::WaterHeaterCallInternal(this);
}

void TemplateWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value()) {
    if (this->optimistic_) {
      this->mode_ = *call.get_mode();
    }
  }
  if (!std::isnan(call.get_target_temperature())) {
    if (this->optimistic_) {
      this->target_temperature_ = call.get_target_temperature();
    }
  }

  this->set_trigger_->trigger();

  if (this->optimistic_) {
    this->publish_state();
  }
}

}  // namespace esphome::template_
