#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../sonoff_spm.h"

namespace esphome {
namespace sonoff_spm {

class SonoffSPMSensor : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_parent(SonoffSPM *parent) { this->parent_ = parent; }
  void set_relay_id(uint8_t relay_id) { this->relay_id_ = relay_id; }

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { this->current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    this->apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_reactive_power_sensor(sensor::Sensor *reactive_power_sensor) {
    this->reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) {
    this->power_factor_sensor_ = power_factor_sensor;
  }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { this->energy_sensor_ = energy_sensor; }

  uint8_t get_relay_id() const { return this->relay_id_; }

 protected:
  void update_sensors_();

  SonoffSPM *parent_{nullptr};
  uint8_t relay_id_{0};

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *reactive_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};

  uint32_t last_update_{0};
};

}  // namespace sonoff_spm
}  // namespace esphome
