#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::template_ {

enum TemplateClimateRestoreMode {
  CLIMATE_NO_RESTORE,
  CLIMATE_RESTORE,
};

class TemplateClimate final : public climate::Climate, public Component {
 public:
  void setup() override;
  void dump_config() override;

  climate::ClimateTraits traits() override { return this->traits_; }

  void set_supports_current_temperature() {
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  void set_supports_current_humidity() { this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY); }

#ifdef USE_SENSOR
  void set_sensor(sensor::Sensor *sensor) {
    this->sensor_ = sensor;
    this->set_supports_current_temperature();
  }
  void set_humidity_sensor(sensor::Sensor *sensor) {
    this->humidity_sensor_ = sensor;
    this->set_supports_current_humidity();
  }
#endif
  void set_supports_action() { this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION); }

  void add_supported_mode(climate::ClimateMode mode) { this->traits_.add_supported_mode(mode); }
  void add_supported_fan_mode(climate::ClimateFanMode mode) { this->traits_.add_supported_fan_mode(mode); }
  void add_supported_swing_mode(climate::ClimateSwingMode mode) { this->traits_.add_supported_swing_mode(mode); }
  void add_supported_preset(climate::ClimatePreset preset) { this->traits_.add_supported_preset(preset); }

  void set_supports_target_humidity() { this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY); }
  void set_supports_two_point_target_temperature() {
    this->traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
  }

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_restore_mode(TemplateClimateRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  // Plain field setters, used both by TemplateClimatePublishAction (a Parented<TemplateClimate>,
  // not a Climate subclass, so it cannot reach the protected validated setters below directly)
  // and by codegen to apply `initial_state:` before setup() runs.
  void set_target_temperature(float value) { this->target_temperature = value; }
  void set_target_temperature_low(float value) { this->target_temperature_low = value; }
  void set_target_temperature_high(float value) { this->target_temperature_high = value; }
  void set_target_humidity(float value) { this->target_humidity = value; }
  void set_mode(climate::ClimateMode mode) { this->mode = mode; }
  void set_swing_mode(climate::ClimateSwingMode mode) { this->swing_mode = mode; }
  void set_fan_mode(climate::ClimateFanMode mode) { this->set_fan_mode_(mode); }
  void set_custom_fan_mode(const char *mode);
  void set_preset(climate::ClimatePreset preset) { this->set_preset_(preset); }
  void set_custom_preset(const char *preset);

 protected:
  void control(const climate::ClimateCall &call) override;

  climate::ClimateTraits traits_;
  bool optimistic_{true};
  TemplateClimateRestoreMode restore_mode_{CLIMATE_RESTORE};

#ifdef USE_SENSOR
  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
#endif
};

}  // namespace esphome::template_
