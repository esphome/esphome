#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace water_heater {

enum WaterHeaterMode : uint8_t {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_HEAT = 1,
  WATER_HEATER_MODE_ECO = 2,
  WATER_HEATER_MODE_BOOST = 3,
};

class WaterHeaterTraits {
 public:
  const std::vector<WaterHeaterMode> &get_supported_modes() const { return supported_modes_; }
  void set_supported_modes(std::vector<WaterHeaterMode> modes) { supported_modes_ = std::move(modes); }

  bool get_supports_current_temperature() const { return supports_current_temperature_; }
  void set_supports_current_temperature(bool v) { supports_current_temperature_ = v; }

  bool get_supports_target_temperature() const { return supports_target_temperature_; }
  void set_supports_target_temperature(bool v) { supports_target_temperature_ = v; }

  float get_visual_min_temperature() const { return visual_min_temperature_; }
  void set_visual_min_temperature(float v) { visual_min_temperature_ = v; }

  float get_visual_max_temperature() const { return visual_max_temperature_; }
  void set_visual_max_temperature(float v) { visual_max_temperature_ = v; }

  float get_visual_temperature_step() const { return visual_temperature_step_; }
  void set_visual_temperature_step(float v) { visual_temperature_step_ = v; }

 private:
  std::vector<WaterHeaterMode> supported_modes_{WATER_HEATER_MODE_OFF, WATER_HEATER_MODE_HEAT};
  bool supports_current_temperature_{true};
  bool supports_target_temperature_{true};

  float visual_min_temperature_{40.0f};
  float visual_max_temperature_{75.0f};
  float visual_temperature_step_{0.5f};
};

class WaterHeater : public EntityBase, public Component {
 public:
  WaterHeaterTraits traits;

  void set_current_temperature_sensor(sensor::Sensor *s) { current_temperature_sensor_ = s; }
  void set_target_temperature_sensor(sensor::Sensor *s) { target_temperature_sensor_ = s; }

  void set_mode(WaterHeaterMode m);
  void set_target_temperature(float t);

  void setup() override {}
  void loop() override;
  void dump_config() override;

  float current_temperature{NAN};
  float target_temperature{NAN};
  WaterHeaterMode mode{WATER_HEATER_MODE_OFF};

 protected:
  void update_values_from_sensors_();

  sensor::Sensor *current_temperature_sensor_{nullptr};
  sensor::Sensor *target_temperature_sensor_{nullptr};
};

}  // namespace water_heater
}  // namespace esphome
