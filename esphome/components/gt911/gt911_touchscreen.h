#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace gt911 {

struct Store {
  volatile bool available;

  static void gpio_intr(Store *store);
};

class GT911Touchscreen : public touchscreen::Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  InternalGPIOPin *interrupt_pin_;
  Store store_;
};

}  // namespace gt911
}  // namespace esphome
