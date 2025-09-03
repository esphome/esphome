#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace as5048b {

class AS5048bComponent : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  /// Setup the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor value (wind angle).
  void update() override;

  float get_setup_priority() const override;

 protected:
  void read_angle_();

  bool data_ready_ = false;
};

}  // namespace as5048b
}  // namespace esphome
