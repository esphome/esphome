#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace gp8403 {

enum GP8403Voltage {
  GP8403_VOLTAGE_5V = 0x00,
  GP8403_VOLTAGE_10V = 0x11,
};

class GP8403 : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_voltage(gp8403::GP8403Voltage voltage) { this->voltage_ = voltage; }

 protected:
  GP8403Voltage voltage_;
};

}  // namespace gp8403
}  // namespace esphome
