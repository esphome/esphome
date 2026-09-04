#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::template_ {

enum class TemplateClimateRestoreMode {
  TEMPLATE_CLIMATE_RESTORE_MODE_NO_RESTORE,
  TEMPLATE_CLIMATE_RESTORE_MODE_RESTORE,
};

class TemplateClimate final : public climate::Climate, public Component {
 public:
  void setup() override;
  void dump_config() override;

  climate::ClimateTraits traits() override { return this->traits_; }

  void add_feature_flags(uint32_t flags) { this->traits_.add_feature_flags(flags); }

#ifdef USE_SENSOR
  // The matching feature flag is added from codegen, so the configuration alone decides it.
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
#endif

  void add_supported_mode(climate::ClimateMode mode) { this->traits_.add_supported_mode(mode); }
  void add_supported_fan_mode(climate::ClimateFanMode mode) { this->traits_.add_supported_fan_mode(mode); }
  void add_supported_swing_mode(climate::ClimateSwingMode mode) { this->traits_.add_supported_swing_mode(mode); }
  void add_supported_preset(climate::ClimatePreset preset) { this->traits_.add_supported_preset(preset); }

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_restore_mode(TemplateClimateRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  // Fired from control() for each field the call carries, so a device-backed config can forward
  // it on. Which of these are configured also decides the two-point/target-humidity traits.
  Trigger<climate::ClimateMode> *get_set_mode_trigger() { return &this->set_mode_trigger_; }
  Trigger<float> *get_set_target_temperature_trigger() { return &this->set_target_temperature_trigger_; }
  Trigger<float> *get_set_target_temperature_low_trigger() { return &this->set_target_temperature_low_trigger_; }
  Trigger<float> *get_set_target_temperature_high_trigger() { return &this->set_target_temperature_high_trigger_; }
  Trigger<float> *get_set_target_humidity_trigger() { return &this->set_target_humidity_trigger_; }
  Trigger<climate::ClimateFanMode> *get_set_fan_mode_trigger() { return &this->set_fan_mode_trigger_; }
  Trigger<StringRef> *get_set_custom_fan_mode_trigger() { return &this->set_custom_fan_mode_trigger_; }
  Trigger<climate::ClimateSwingMode> *get_set_swing_mode_trigger() { return &this->set_swing_mode_trigger_; }
  Trigger<climate::ClimatePreset> *get_set_preset_trigger() { return &this->set_preset_trigger_; }
  Trigger<StringRef> *get_set_custom_preset_trigger() { return &this->set_custom_preset_trigger_; }

  // Used by TemplateClimatePublishAction, which is not a Climate subclass and so cannot reach the
  // protected setters, and by codegen to apply `initial_state:` before setup() runs.
  void set_target_temperature(float value) { this->target_temperature = value; }
  void set_target_temperature_low(float value) { this->target_temperature_low = value; }
  void set_target_temperature_high(float value) { this->target_temperature_high = value; }
  void set_target_humidity(float value) { this->target_humidity = value; }
  void set_mode(climate::ClimateMode mode);
  void set_swing_mode(climate::ClimateSwingMode mode);
  void set_fan_mode(climate::ClimateFanMode mode);
  void set_custom_fan_mode(const char *mode) { this->set_custom_fan_mode(StringRef(mode)); }
  void set_custom_fan_mode(StringRef mode);
  void set_preset(climate::ClimatePreset preset);
  void set_custom_preset(const char *preset) { this->set_custom_preset(StringRef(preset)); }
  void set_custom_preset(StringRef preset);

 protected:
  void control(const climate::ClimateCall &call) override;

  climate::ClimateTraits traits_;
  bool optimistic_{false};
  TemplateClimateRestoreMode restore_mode_{TemplateClimateRestoreMode::TEMPLATE_CLIMATE_RESTORE_MODE_NO_RESTORE};

#ifdef USE_SENSOR
  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
#endif

  Trigger<climate::ClimateMode> set_mode_trigger_;
  Trigger<float> set_target_temperature_trigger_;
  Trigger<float> set_target_temperature_low_trigger_;
  Trigger<float> set_target_temperature_high_trigger_;
  Trigger<float> set_target_humidity_trigger_;
  Trigger<climate::ClimateFanMode> set_fan_mode_trigger_;
  Trigger<StringRef> set_custom_fan_mode_trigger_;
  Trigger<climate::ClimateSwingMode> set_swing_mode_trigger_;
  Trigger<climate::ClimatePreset> set_preset_trigger_;
  Trigger<StringRef> set_custom_preset_trigger_;
};

}  // namespace esphome::template_
