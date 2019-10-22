#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sts3x {

/// This class implements support for the ST3x-DIS family of temperature i2c sensors.
class STS3XComponent : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);
};

}  // namespace sts3x
}  // namespace esphome
