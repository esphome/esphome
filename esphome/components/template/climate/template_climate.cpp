#include "template_climate.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.climate";

void TemplateClimate::setup() {
  if (this->restore_mode_ == TemplateClimateRestoreMode::TEMPLATE_CLIMATE_RESTORE_MODE_RESTORE) {
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(this);
    }
  }

  // Sensors publish every reading, not just changes, so only re-publish when the value moved.
  // NAN means the sensor went unavailable and is passed through rather than dropped; the second
  // check stops an unavailable sensor re-publishing forever, since NAN never equals NAN.
#ifdef USE_SENSOR
  if (this->sensor_ != nullptr) {
    this->current_temperature = this->sensor_->state;
    this->sensor_->add_on_state_callback([this](float state) {
      if (state != this->current_temperature && !(std::isnan(state) && std::isnan(this->current_temperature))) {
        this->current_temperature = state;
        this->publish_state();
      }
    });
  }

  if (this->humidity_sensor_ != nullptr) {
    this->current_humidity = this->humidity_sensor_->state;
    this->humidity_sensor_->add_on_state_callback([this](float state) {
      if (state != this->current_humidity && !(std::isnan(state) && std::isnan(this->current_humidity))) {
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
  // Each field present fires its set_*_action; on_control sees the whole call. optimistic: true
  // also applies the values right away, false waits for a climate.template.publish report.
  if (auto mode = call.get_mode()) {
    if (this->optimistic_)
      this->mode = *mode;
    this->set_mode_trigger_.trigger(*mode);
  }

  if (auto target_temp = call.get_target_temperature()) {
    if (this->optimistic_)
      this->target_temperature = *target_temp;
    this->set_target_temperature_trigger_.trigger(*target_temp);
  }

  if (auto target_temp_low = call.get_target_temperature_low()) {
    if (this->optimistic_)
      this->target_temperature_low = *target_temp_low;
    this->set_target_temperature_low_trigger_.trigger(*target_temp_low);
  }

  if (auto target_temp_high = call.get_target_temperature_high()) {
    if (this->optimistic_)
      this->target_temperature_high = *target_temp_high;
    this->set_target_temperature_high_trigger_.trigger(*target_temp_high);
  }

  if (auto target_humidity = call.get_target_humidity()) {
    if (this->optimistic_)
      this->target_humidity = *target_humidity;
    this->set_target_humidity_trigger_.trigger(*target_humidity);
  }

  if (auto fan_mode = call.get_fan_mode()) {
    if (this->optimistic_)
      this->set_fan_mode_(*fan_mode);
    this->set_fan_mode_trigger_.trigger(*fan_mode);
  }

  if (call.has_custom_fan_mode()) {
    if (this->optimistic_)
      this->set_custom_fan_mode_(call.get_custom_fan_mode());
    this->set_custom_fan_mode_trigger_.trigger(call.get_custom_fan_mode());
  }

  if (auto swing_mode = call.get_swing_mode()) {
    if (this->optimistic_)
      this->swing_mode = *swing_mode;
    this->set_swing_mode_trigger_.trigger(*swing_mode);
  }

  if (auto preset = call.get_preset()) {
    if (this->optimistic_)
      this->set_preset_(*preset);
    this->set_preset_trigger_.trigger(*preset);
  }

  if (call.has_custom_preset()) {
    if (this->optimistic_)
      this->set_custom_preset_(call.get_custom_preset());
    this->set_custom_preset_trigger_.trigger(call.get_custom_preset());
  }

  if (this->optimistic_)
    this->publish_state();
}

// A climate.template.publish report (and initial_state:) never goes through ClimateCall::validate_(),
// so check here instead -- otherwise a typo is published as state the receiving end will reject.
void TemplateClimate::set_mode(climate::ClimateMode mode) {
  if (!this->traits_.supports_mode(mode)) {
    ESP_LOGW(TAG, "'%s' - Unsupported mode %u", this->get_name().c_str(), static_cast<unsigned>(mode));
    return;
  }
  this->mode = mode;
}

void TemplateClimate::set_swing_mode(climate::ClimateSwingMode swing_mode) {
  if (!this->traits_.supports_swing_mode(swing_mode)) {
    ESP_LOGW(TAG, "'%s' - Unsupported swing mode %u", this->get_name().c_str(), static_cast<unsigned>(swing_mode));
    return;
  }
  this->swing_mode = swing_mode;
}

void TemplateClimate::set_fan_mode(climate::ClimateFanMode fan_mode) {
  if (!this->traits_.supports_fan_mode(fan_mode)) {
    ESP_LOGW(TAG, "'%s' - Unsupported fan mode %u", this->get_name().c_str(), static_cast<unsigned>(fan_mode));
    return;
  }
  this->set_fan_mode_(fan_mode);
}

void TemplateClimate::set_preset(climate::ClimatePreset preset) {
  if (!this->traits_.supports_preset(preset)) {
    ESP_LOGW(TAG, "'%s' - Unsupported preset %u", this->get_name().c_str(), static_cast<unsigned>(preset));
    return;
  }
  this->set_preset_(preset);
}

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
