#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace gt911 {

class GT911ButtonListener {
 public:
  virtual void update_button(uint8_t index, bool state) = 0;
};

class GT911Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void register_button_listener(GT911ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  std::vector<GT911ButtonListener *> button_listeners_;
  uint8_t button_state_{0xFF};  // last button state. Initial FF guarantees first update.
};

}  // namespace gt911
}  // namespace esphome
