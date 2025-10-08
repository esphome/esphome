#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace max17043 {

class MAX17043Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void sleep_mode();

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) {
    battery_remaining_sensor_ = battery_remaining_sensor;
  }

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
};

}  // namespace max17043
}  // namespace esphome
