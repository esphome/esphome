#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace stts22h {

class STTS22HComponent : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  uint8_t read_sensor_identification();
  void enable_low_odr_operation_mode();
  void enable_reg_adr_auto_increment();
  float read_temperature();
};

}  // namespace stts22h
}  // namespace esphome
