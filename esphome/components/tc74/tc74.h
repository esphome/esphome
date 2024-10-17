#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tc74 {

class TC74Component : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  /// Setup the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor value (temperature).
  void update() override;

  float get_setup_priority() const override;

 protected:
  /// Internal method to read the temperature from the component after it has been scheduled.
  void read_temperature_();

  bool data_ready_ = false;
};

}  // namespace tc74
}  // namespace esphome
