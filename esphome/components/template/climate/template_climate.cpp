#include "template_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template_climate";

TemplateClimate::TemplateClimate()
    : set_mode_trigger_(new Trigger<climate::ClimateMode>()),
      set_target_temperature_trigger_(new Trigger<float>()),
      set_target_temperature_low_trigger_(new Trigger<float>()),
      set_target_temperature_high_trigger_(new Trigger<float>()),
      set_target_humidity_trigger_(new Trigger<float>()),
      set_fan_mode_trigger_(new Trigger<climate::ClimateFanMode>()),
      set_custom_fan_mode_trigger_(new Trigger<std::string>()),
      set_swing_mode_trigger_(new Trigger<climate::ClimateSwingMode>()),
      set_preset_trigger_(new Trigger<climate::ClimatePreset>()),
      set_custom_preset_trigger_(new Trigger<std::string>()) {}

void TemplateClimate::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }
  if (this->current_temperature_f_.has_value()) {
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  if (this->current_humidity_f_.has_value()) {
    this->traits_.set_supports_current_humidity(true);
  }
  if (this->target_humidity_f_.has_value()) {
    this->traits_.set_supports_target_humidity(true);
  }
  if (this->target_temperature_low_f_.has_value()) {
    this->traits_.set_supports_two_point_target_temperature(true);
  }
  if (this->action_f_.has_value()) {
    this->traits_.set_supports_action(true);
  }
  if (!this->current_temperature_f_.has_value() && !this->current_humidity_f_.has_value() &&
      !this->target_temperature_f_.has_value() && !this->target_temperature_low_f_.has_value() &&
      !this->target_temperature_high_f_.has_value() && !this->target_humidity_f_.has_value() &&
      !this->mode_f_.has_value() && !this->action_f_.has_value() && !this->fan_mode_f_.has_value() &&
      !this->custom_fan_mode_f_.has_value() && !this->swing_mode_f_.has_value() && !this->preset_f_.has_value() &&
      !this->custom_preset_f_.has_value()) {
    this->disable_loop();
  }
}

void TemplateClimate::loop() {
  bool changed = false;

  if (auto val = this->current_temperature_f_()) {
    if (!std::isnan(*val) && *val != this->current_temperature) {
      this->current_temperature = *val;
      changed = true;
    }
  }

  if (auto val = this->current_humidity_f_()) {
    if (!std::isnan(*val) && *val != this->current_humidity) {
      this->current_humidity = *val;
      changed = true;
    }
  }

  if (auto val = this->target_temperature_f_()) {
    if (!std::isnan(*val) && *val != this->target_temperature) {
      this->target_temperature = *val;
      changed = true;
    }
  }

  if (auto val = this->target_temperature_low_f_()) {
    if (!std::isnan(*val) && *val != this->target_temperature_low) {
      this->target_temperature_low = *val;
      changed = true;
    }
  }

  if (auto val = this->target_temperature_high_f_()) {
    if (!std::isnan(*val) && *val != this->target_temperature_high) {
      this->target_temperature_high = *val;
      changed = true;
    }
  }

  if (auto val = this->target_humidity_f_()) {
    if (!std::isnan(*val) && *val != this->target_humidity) {
      this->target_humidity = *val;
      changed = true;
    }
  }

  if (auto val = this->mode_f_()) {
    if (*val != this->mode) {
      this->mode = *val;
      changed = true;
    }
  }

  if (auto val = this->action_f_()) {
    if (*val != this->action) {
      this->action = *val;
      changed = true;
    }
  }

  if (auto val = this->fan_mode_f_()) {
    if (this->fan_mode != val) {
      this->fan_mode = val;
      changed = true;
    }
  }

  if (auto val = this->custom_fan_mode_f_()) {
    if (this->get_custom_fan_mode() != *val) {
      if (this->set_custom_fan_mode_(val->c_str()))
        changed = true;
    }
  }

  if (auto val = this->swing_mode_f_()) {
    if (*val != this->swing_mode) {
      this->swing_mode = *val;
      changed = true;
    }
  }

  if (auto val = this->preset_f_()) {
    if (this->preset != val) {
      this->preset = val;
      changed = true;
    }
  }

  if (auto val = this->custom_preset_f_()) {
    if (this->get_custom_preset() != *val) {
      if (this->set_custom_preset_(val->c_str()))
        changed = true;
    }
  }

  if (changed)
    this->publish_state();
}

void TemplateClimate::dump_config() {
  LOG_CLIMATE("", "Template Climate", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

void TemplateClimate::control(const climate::ClimateCall &call) {
  // Read lambdas (loop) poll the device state each iteration.
  // Set actions send new settings to the controlled device.
  // optimistic=true:  also update the entity state immediately after an action,
  //   so the UI reflects the change right away; the next loop() read will
  //   confirm or correct it.
  // optimistic=false: do not update the entity state after an action; wait for
  //   the next loop() read lambda to reflect the device's actual state.
  // When no read lambdas are configured, always update entity state immediately
  // since nothing will otherwise update it.
  const bool has_state_lambdas =
      this->target_temperature_f_.has_value() || this->target_temperature_low_f_.has_value() ||
      this->target_temperature_high_f_.has_value() || this->target_humidity_f_.has_value() ||
      this->mode_f_.has_value() || this->action_f_.has_value() || this->fan_mode_f_.has_value() ||
      this->custom_fan_mode_f_.has_value() || this->swing_mode_f_.has_value() || this->preset_f_.has_value() ||
      this->custom_preset_f_.has_value();
  const bool apply_state = !has_state_lambdas || this->optimistic_;

  if (auto mode = call.get_mode()) {
    if (apply_state)
      this->mode = *mode;
    this->set_mode_trigger_->trigger(*mode);
  }

  if (auto target_temp = call.get_target_temperature()) {
    if (apply_state)
      this->target_temperature = *target_temp;
    this->set_target_temperature_trigger_->trigger(*target_temp);
  }

  if (auto target_temp_low = call.get_target_temperature_low()) {
    if (apply_state)
      this->target_temperature_low = *target_temp_low;
    this->set_target_temperature_low_trigger_->trigger(*target_temp_low);
  }

  if (auto target_temp_high = call.get_target_temperature_high()) {
    if (apply_state)
      this->target_temperature_high = *target_temp_high;
    this->set_target_temperature_high_trigger_->trigger(*target_temp_high);
  }

  if (auto target_humidity = call.get_target_humidity()) {
    if (apply_state)
      this->target_humidity = *target_humidity;
    this->set_target_humidity_trigger_->trigger(*target_humidity);
  }

  if (auto fan_mode = call.get_fan_mode()) {
    if (apply_state)
      this->fan_mode = fan_mode;
    this->set_fan_mode_trigger_->trigger(*fan_mode);
  }

  if (call.has_custom_fan_mode()) {
    if (apply_state)
      this->set_custom_fan_mode_(call.get_custom_fan_mode());
    this->set_custom_fan_mode_trigger_->trigger(std::string(call.get_custom_fan_mode()));
  }

  if (auto swing_mode = call.get_swing_mode()) {
    if (apply_state)
      this->swing_mode = *swing_mode;
    this->set_swing_mode_trigger_->trigger(*swing_mode);
  }

  if (auto preset = call.get_preset()) {
    if (apply_state)
      this->preset = preset;
    this->set_preset_trigger_->trigger(*preset);
  }

  if (call.has_custom_preset()) {
    if (apply_state)
      this->set_custom_preset_(call.get_custom_preset());
    this->set_custom_preset_trigger_->trigger(std::string(call.get_custom_preset()));
  }

  this->publish_state();
}

}  // namespace template_
}  // namespace esphome
