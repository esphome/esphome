#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tt21100 {

using namespace touchscreen;

struct TT21100TouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(TT21100TouchscreenStore *store);
};

class TT21100ButtonListener {
 public:
  virtual void update_button(uint8_t index, uint16_t state) = 0;
};

class TT21100Touchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

  void register_button_listener(TT21100ButtonListener *listener) { this->button_listeners_.push_back(listener); }

 protected:
  void reset_();

  TT21100TouchscreenStore store_;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_{nullptr};

  std::vector<TT21100ButtonListener *> button_listeners_;
};

}  // namespace tt21100
}  // namespace esphome
