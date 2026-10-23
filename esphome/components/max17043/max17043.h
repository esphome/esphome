#pragma once

#include "esphome/components/max170xx/max170xx.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::max17043 {

class MAX17043Component final : public max170xx::MAX170xxComponent<max170xx::Model::MODEL_MAX17043> {
 public:
  void dump_config() override;
  void update() override;

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) {
    this->battery_remaining_sensor_ = battery_remaining_sensor;
  }

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
};

}  // namespace esphome::max17043
