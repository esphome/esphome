#include "template_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template_climate";

TemplateClimate::TemplateClimate()
    : set_mode_trigger_(new Trigger<climate::ClimateMode>()),
      set_target_temperature_trigger_(new Trigger<float>()),
      set_fan_mode_trigger_(new Trigger<climate::ClimateFanMode>()),
      set_swing_mode_trigger_(new Trigger<climate::ClimateSwingMode>()),
      set_preset_trigger_(new Trigger<climate::ClimatePreset>()) {}

void TemplateClimate::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }
  if (!this->current_temperature_f_.has_value() && !this->target_temperature_f_.has_value()) {
    this->disable_loop();
  }
}

void TemplateClimate::loop() {
  bool changed = false;

  if (auto val = this->current_temperature_f_()) {
    if (*val != this->current_temperature) {
      this->current_temperature = *val;
      changed = true;
    }
  }

  if (auto val = this->target_temperature_f_()) {
    if (*val != this->target_temperature) {
      this->target_temperature = *val;
      changed = true;
    }
  }

  if (changed)
    this->publish_state();
}

void TemplateClimate::dump_config() { LOG_CLIMATE("", "Template Climate", this); }

void TemplateClimate::control(const climate::ClimateCall &call) {
  if (auto mode = call.get_mode()) {
    if (this->optimistic_) {
      this->mode = *mode;
    }
    this->set_mode_trigger_->trigger(*mode);
  }

  if (auto target_temp = call.get_target_temperature()) {
    if (this->optimistic_) {
      this->target_temperature = *target_temp;
    }
    this->set_target_temperature_trigger_->trigger(*target_temp);
  }

  if (auto fan_mode = call.get_fan_mode()) {
    if (this->optimistic_) {
      this->fan_mode = fan_mode;
    }
    this->set_fan_mode_trigger_->trigger(*fan_mode);
  }

  if (auto swing_mode = call.get_swing_mode()) {
    if (this->optimistic_) {
      this->swing_mode = *swing_mode;
    }
    this->set_swing_mode_trigger_->trigger(*swing_mode);
  }

  if (auto preset = call.get_preset()) {
    if (this->optimistic_) {
      this->preset = preset;
    }
    this->set_preset_trigger_->trigger(*preset);
  }

  this->publish_state();
}

}  // namespace template_
}  // namespace esphome
