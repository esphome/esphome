#pragma once

#ifdef USE_ARDUINO
#include "esphome/core/esphal.h"
#include <esp32-hal-gpio.h>

namespace esphome {
namespace esp32 {

class ArduinoInternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override {
    pin_mode(flags_);
  }
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override {
    return bool(digitalRead(pin_)) != inverted_;
  }
  void digital_write(bool value)  override {
    digitalWrite(pin_, value != inverted_ ? 1 : 0);
  }
  std::string dump_summary() const override;
  void detach_interrupt() const override {
    detachInterrupt(pin_);
  }
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return pin_; }

 protected:
  void attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace esp32
}  // namespace esphome

#endif  // USE_ARDUINO
