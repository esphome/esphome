#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tmp102 {

class TMP102Component : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  /// Setup (reset) the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor values (temperature)
  void update() override;

  float get_setup_priority() const override;
};

}  // namespace tmp102
}  // namespace esphome
