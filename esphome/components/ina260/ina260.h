#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ina260 {

class INA260Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_bus_voltage_sensor(sensor::Sensor *bus_voltage_sensor) { this->bus_voltage_sensor_ = bus_voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { this->current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }

 protected:
  uint16_t manufacture_id_{0};
  uint16_t device_id_{0};

  sensor::Sensor *bus_voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};

  enum ErrorCode {
    NONE,
    COMMUNICATION_FAILED,
    DEVICE_RESET_FAILED,
    FAILED_TO_UPDATE_CONFIGURATION,
  } error_code_{NONE};
};

}  // namespace ina260
}  // namespace esphome
