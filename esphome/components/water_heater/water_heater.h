#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "water_heater_traits.h"

namespace esphome {
namespace water_heater {

class WaterHeater : public EntityBase, public Component {
 public:
  void set_current_temperature_sensor(sensor::Sensor *s) { current_temperature_sensor_ = s; }
  void set_target_temperature_sensor(sensor::Sensor *s) { target_temperature_sensor_ = s; }

  void set_mode(WaterHeaterMode m);
  void set_target_temperature(float t);

  float get_current_temperature() const { return current_temperature_; }
  float get_target_temperature() const { return target_temperature_; }
  WaterHeaterMode get_mode() const { return mode_; }

  WaterHeaterTraits traits() const { return traits_; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void update_sensor_values_();

  WaterHeaterMode mode_{WATER_HEATER_MODE_OFF};
  float current_temperature_{NAN};
  float target_temperature_{NAN};

  sensor::Sensor *current_temperature_sensor_{nullptr};
  sensor::Sensor *target_temperature_sensor_{nullptr};

  WaterHeaterTraits traits_;
};

}  // namespace water_heater
}  // namespace esphome
