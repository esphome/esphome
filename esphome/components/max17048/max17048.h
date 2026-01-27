#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::max17048 {
class MAX17048Component : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_battery_v_sensor(sensor::Sensor *battery_v_sensor) { this->battery_voltage_sensor_ = battery_v_sensor; }
  void set_battery_soc_sensor(sensor::Sensor *battery_soc_sensor) { this->battery_soc_sensor_ = battery_soc_sensor; }
  void set_battery_soc_rate_sensor(sensor::Sensor *battery_soc_rate_sensor) {
    this->battery_soc_rate_sensor_ = battery_soc_rate_sensor;
  }

 protected:
  sensor::Sensor *battery_voltage_sensor_;
  sensor::Sensor *battery_soc_sensor_;
  sensor::Sensor *battery_soc_rate_sensor_;
  void initialize_sensor_();
};
}  // namespace esphome::max17048
