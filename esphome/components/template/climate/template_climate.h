#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::template_ {

class TemplateClimate final : public climate::Climate, public Component {
 public:
  void setup() override;
  void dump_config() override;

  climate::ClimateTraits traits() override { return this->traits_; }

  void set_sensor(sensor::Sensor *sensor) {
    this->sensor_ = sensor;
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  void set_humidity_sensor(sensor::Sensor *sensor) {
    this->humidity_sensor_ = sensor;
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  }
  void set_supports_action() { this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION); }

  void add_supported_mode(climate::ClimateMode mode) { this->traits_.add_supported_mode(mode); }
  void add_supported_fan_mode(climate::ClimateFanMode mode) { this->traits_.add_supported_fan_mode(mode); }
  void add_supported_swing_mode(climate::ClimateSwingMode mode) { this->traits_.add_supported_swing_mode(mode); }
  void add_supported_preset(climate::ClimatePreset preset) { this->traits_.add_supported_preset(preset); }

  // Called at codegen time from the supports_target_humidity/supports_two_point_target_temperature
  // config booleans, since these features can no longer be inferred from lambda presence.
  void set_supports_target_humidity() { this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY); }
  void set_supports_two_point_target_temperature() {
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
  }

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

  // Public wrappers for TemplateClimatePublishAction (a Parented<TemplateClimate>, not a Climate
  // subclass, so it cannot reach the protected validated setters below directly). Each sets the
  // field/validated-state directly; publish_state() is called once by the action after all
  // configured fields for that invocation have been applied. This bypasses control()/ClimateCall/
  // on_control entirely, by design: this represents the device reporting its actual state, not a
  // new command, so it must not re-trigger on_control as if the entity were being commanded.
  void publish_current_temperature(float value) { this->current_temperature = value; }
  void publish_current_humidity(float value) { this->current_humidity = value; }
  void publish_target_temperature(float value) { this->target_temperature = value; }
  void publish_target_temperature_low(float value) { this->target_temperature_low = value; }
  void publish_target_temperature_high(float value) { this->target_temperature_high = value; }
  void publish_target_humidity(float value) { this->target_humidity = value; }
  void publish_mode(climate::ClimateMode mode) { this->mode = mode; }
  void publish_action(climate::ClimateAction action) { this->action = action; }
  void publish_fan_mode(climate::ClimateFanMode mode) { this->set_fan_mode_(mode); }
  void publish_custom_fan_mode(const std::string &mode) { this->set_custom_fan_mode_(mode.c_str(), mode.size()); }
  void publish_swing_mode(climate::ClimateSwingMode mode) { this->swing_mode = mode; }
  void publish_preset(climate::ClimatePreset preset) { this->set_preset_(preset); }
  void publish_custom_preset(const std::string &preset) { this->set_custom_preset_(preset.c_str(), preset.size()); }

 protected:
  void control(const climate::ClimateCall &call) override;

  climate::ClimateTraits traits_;
  bool optimistic_{true};

  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace esphome::template_
