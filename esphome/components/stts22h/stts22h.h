#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace stts22h {

class STTS22H : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  float read_temperature();
  i2c::ErrorCode configure_sensor();
};

}  // namespace stts22h
}  // namespace esphome
