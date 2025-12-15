#include "template_water_heater.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <set>

namespace esphome::template_ {

static const char *const TAG = "template.water_heater";

TemplateWaterHeater::TemplateWaterHeater() {}

void TemplateWaterHeater::setup() {
  if (this->restore_mode_ == TemplateWaterHeaterRestoreMode::WATER_HEATER_RESTORE ||
      this->restore_mode_ == TemplateWaterHeaterRestoreMode::WATER_HEATER_RESTORE_AND_CALL) {
    auto restore = this->restore_state();

    if (restore.has_value()) {
      if (this->restore_mode_ == TemplateWaterHeaterRestoreMode::WATER_HEATER_RESTORE) {
        restore->apply(this);
      } else {
        restore->to_call(this).perform();
      }
    }
  }
}

void TemplateWaterHeater::loop() {
  bool changed = false;

  auto curr_temp = this->current_temperature_f_.call();
  if (curr_temp.has_value()) {
    if (*curr_temp != this->current_temperature) {
      this->current_temperature = *curr_temp;
      changed = true;
    }
  }

  auto new_mode = this->mode_f_.call();
  if (new_mode.has_value()) {
    if (*new_mode != this->mode) {
      this->mode = *new_mode;
      changed = true;
    }
  }

  if (changed) {
    this->publish_state();
  }
}

void TemplateWaterHeater::dump_config() {
  LOG_WATER_HEATER(TAG, "Template Water Heater", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  ESP_LOGCONFIG(TAG, "  Min Temperature: %.1f", this->min_temperature_);
  ESP_LOGCONFIG(TAG, "  Max Temperature: %.1f", this->max_temperature_);
}

float TemplateWaterHeater::get_setup_priority() const { return setup_priority::HARDWARE; }

water_heater::WaterHeaterCallInternal TemplateWaterHeater::make_call() {
  return water_heater::WaterHeaterCallInternal(this);
}

water_heater::WaterHeaterTraits TemplateWaterHeater::traits() {
  auto traits = water_heater::WaterHeaterTraits();
  traits.set_supports_current_temperature(true);
  traits.set_min_temperature(this->min_temperature_);
  traits.set_max_temperature(this->max_temperature_);

  traits.set_supported_modes({water_heater::WATER_HEATER_MODE_OFF, water_heater::WATER_HEATER_MODE_ECO,
                              water_heater::WATER_HEATER_MODE_ELECTRIC, water_heater::WATER_HEATER_MODE_PERFORMANCE,
                              water_heater::WATER_HEATER_MODE_HIGH_DEMAND, water_heater::WATER_HEATER_MODE_HEAT_PUMP,
                              water_heater::WATER_HEATER_MODE_GAS});
  return traits;
}

void TemplateWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value()) {
    if (this->optimistic_) {
      this->mode = *call.get_mode();
    }
  }
  if (call.get_target_temperature().has_value()) {
    if (this->optimistic_) {
      this->target_temperature = *call.get_target_temperature();
    }
  }

  this->set_trigger_->trigger();

  if (this->optimistic_) {
    this->publish_state();
  }
}

}  // namespace template_
}  // namespace esphome
