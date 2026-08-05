#include "template_climate.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template_climate";

void TemplateClimate::setup() {
  if (this->restore_mode_ == TemplateClimateRestoreMode::RESTORE) {
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(this);
    }
  }

  // Sensors publish on every reading, not just on change, so only re-publish the climate state
  // when the value actually moved -- otherwise a fast-polling backing sensor would spam
  // publish_state() with no new information.
#ifdef USE_SENSOR
  if (this->sensor_ != nullptr) {
    this->current_temperature = this->sensor_->state;
    this->sensor_->add_on_state_callback([this](float state) {
      if (!std::isnan(state) && state != this->current_temperature) {
        this->current_temperature = state;
        this->publish_state();
      }
    });
  }

  if (this->humidity_sensor_ != nullptr) {
    this->current_humidity = this->humidity_sensor_->state;
    this->humidity_sensor_->add_on_state_callback([this](float state) {
      if (!std::isnan(state) && state != this->current_humidity) {
        this->current_humidity = state;
        this->publish_state();
      }
    });
  }
#endif
}

void TemplateClimate::dump_config() {
  LOG_CLIMATE("", "Template Climate", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

void TemplateClimate::control(const climate::ClimateCall &call) {
  // optimistic: true applies the requested values immediately; optimistic: false leaves them
  // untouched until a climate.template.publish action reports the device's actual state.
  // on_control (from the base Climate component) is what a real device-backed config uses to
  // forward this call's contents to the physical device.
  if (this->optimistic_) {
    if (auto mode = call.get_mode())
      this->mode = *mode;

    if (auto target_temp = call.get_target_temperature())
      this->target_temperature = *target_temp;

    if (auto target_temp_low = call.get_target_temperature_low())
      this->target_temperature_low = *target_temp_low;

    if (auto target_temp_high = call.get_target_temperature_high())
      this->target_temperature_high = *target_temp_high;

    if (auto target_humidity = call.get_target_humidity())
      this->target_humidity = *target_humidity;

    if (auto fan_mode = call.get_fan_mode())
      this->set_fan_mode_(*fan_mode);

    if (call.has_custom_fan_mode())
      this->set_custom_fan_mode_(call.get_custom_fan_mode());

    if (auto swing_mode = call.get_swing_mode())
      this->swing_mode = *swing_mode;

    if (auto preset = call.get_preset())
      this->set_preset_(*preset);

    if (call.has_custom_preset())
      this->set_custom_preset_(call.get_custom_preset());

    this->publish_state();
  }
}

// Unlike control(), a climate.template.publish report (and initial_state:) never goes through
// ClimateCall::validate_(), so an unsupported custom mode isn't caught anywhere upstream -- check
// it here (the same lookup set_custom_fan_mode_()/set_custom_preset_() use internally) so a
// typo'd custom_fan_mode/custom_preset doesn't get silently dropped.
void TemplateClimate::set_custom_fan_mode(StringRef mode) {
  if (this->find_custom_fan_mode_(mode.c_str(), mode.size()) == nullptr) {
    ESP_LOGW(TAG, "'%s' - Unsupported custom fan mode '%s'", this->get_name().c_str(), mode.c_str());
    return;
  }
  this->set_custom_fan_mode_(mode);
}

void TemplateClimate::set_custom_preset(StringRef preset) {
  if (this->find_custom_preset_(preset.c_str(), preset.size()) == nullptr) {
    ESP_LOGW(TAG, "'%s' - Unsupported custom preset '%s'", this->get_name().c_str(), preset.c_str());
    return;
  }
  this->set_custom_preset_(preset);
}

}  // namespace esphome::template_
