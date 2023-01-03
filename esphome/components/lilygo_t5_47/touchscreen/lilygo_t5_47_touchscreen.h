#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lilygo_t5_47 {

struct Store {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(Store *store);
};

using namespace touchscreen;

class LilygoT547Touchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  InternalGPIOPin *interrupt_pin_;
  Store store_;
};

}  // namespace lilygo_t5_47
}  // namespace esphome
