#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace gp8211 {

enum GP8211Voltage {
  GP8211_VOLTAGE_5V = 0x00,
  GP8211_VOLTAGE_10V = 0x11,
};

class GP8211 : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_voltage(gp8211::GP8211Voltage voltage) { this->voltage_ = voltage; }

 protected:
  GP8211Voltage voltage_;
};

}  // namespace gp8211
}  // namespace esphome
