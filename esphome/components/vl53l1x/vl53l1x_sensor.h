#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

class VL53L1X;

namespace esphome {
namespace vl53l1x {

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L1XSensor();

  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void loop() override;

  void set_signal_rate_limit(float signal_rate_limit) {}
  void set_long_range(bool long_range) {}

  void set_i2c_parent(i2c::I2CComponent* parent);
  void set_i2c_address(uint8_t address);

 protected:
  VL53L1X* vl53l1x_{nullptr};
};

}  // namespace vl53l1x
}  // namespace esphome
