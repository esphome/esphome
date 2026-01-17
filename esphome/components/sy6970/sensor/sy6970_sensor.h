#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../sy6970.h"

namespace esphome::sy6970 {

class SY6970Sensor : public PollingComponent {
 public:
  void set_parent(SY6970Component *parent) { this->parent_ = parent; }
  void set_vbus_voltage_sensor(sensor::Sensor *vbus_voltage_sensor) {
    this->vbus_voltage_sensor_ = vbus_voltage_sensor;
  }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {
    this->battery_voltage_sensor_ = battery_voltage_sensor;
  }
  void set_system_voltage_sensor(sensor::Sensor *system_voltage_sensor) {
    this->system_voltage_sensor_ = system_voltage_sensor;
  }
  void set_charge_current_sensor(sensor::Sensor *charge_current_sensor) {
    this->charge_current_sensor_ = charge_current_sensor;
  }
  void set_precharge_current_sensor(sensor::Sensor *precharge_current_sensor) {
    this->precharge_current_sensor_ = precharge_current_sensor;
  }

  void update() override;

 protected:
  SY6970Component *parent_{nullptr};
  sensor::Sensor *vbus_voltage_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *system_voltage_sensor_{nullptr};
  sensor::Sensor *charge_current_sensor_{nullptr};
  sensor::Sensor *precharge_current_sensor_{nullptr};
};

}  // namespace esphome::sy6970
