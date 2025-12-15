#include "template_water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.water_heater";

TemplateWaterHeater::TemplateWaterHeater() {}

void TemplateWaterHeater::setup() {
  switch (this->restore_mode_) {
    case WATER_HEATER_NO_RESTORE:
      break;
    case WATER_HEATER_RESTORE: {
      auto restore = this->restore_state_();
      if (restore.has_value())
        restore->apply(this);
      break;
    }
    case WATER_HEATER_RESTORE_AND_CALL: {
      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->to_call(this).perform();
      }
      break;
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
  LOG_WATER_HEATER("", "Template Water Heater", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

float TemplateWaterHeater::get_setup_priority() const { return setup_priority::HARDWARE; }

water_heater::WaterHeaterTraits TemplateWaterHeater::traits() {
  auto traits = water_heater::WaterHeaterTraits();
  traits.set_supports_current_temperature(true);
  traits.set_min_temperature(10.0);
  traits.set_max_temperature(90.0);

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