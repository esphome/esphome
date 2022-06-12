#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace es8388 {

class ES8388Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void set_power_pin(GPIOPin *pin) { this->power_pin_ = pin; }

 protected:
  GPIOPin *power_pin_;
};

}  // namespace es8388
}  // namespace esphome
