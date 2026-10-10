#pragma once

#ifdef USE_HOST

#include "esphome/core/gpio_pin.h"

namespace esphome::host {

class HostGPIOPin final : public GPIOPin {
 public:
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override { pin_mode(flags_); }
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;
  void detach_interrupt() const;
  template<typename T> void attach_interrupt(void (*func)(T *), T *arg, gpio::InterruptType type) const {
    this->attach_interrupt_(reinterpret_cast<void (*)(void *)>(func), arg, type);
  }
  ISRInternalGPIOPin to_isr() const;
  uint8_t get_pin() const { return pin_; }
  gpio::Flags get_flags() const override { return flags_; }
  bool is_inverted() const { return inverted_; }
  bool is_internal() override { return true; }

 protected:
  void attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const;

  uint8_t pin_;
  bool inverted_{};
  gpio::Flags flags_{};
};

}  // namespace esphome::host

#endif  // USE_HOST
