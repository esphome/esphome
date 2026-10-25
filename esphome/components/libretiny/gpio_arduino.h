#pragma once

#ifdef USE_LIBRETINY
#include "esphome/core/gpio_pin.h"

namespace esphome::libretiny {

class ArduinoInternalGPIOPin final : public GPIOPin {
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
  // The ISR copy of the pin goes through these too, so both see the same mode and level
  friend class ::esphome::ISRInternalGPIOPin;
  void set_mode_(gpio::Flags flags);
  void write_(bool level);
  bool read_();
  void attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const;

  uint8_t pin_;
  bool inverted_ : 1 {};
  // On LibreTiny a write to a pin that is not an output only changes its pull, so a level written before the
  // pin becomes an output is kept here and applied by set_mode_()
  bool output_ : 1 {};
  bool level_ : 1 {};
  bool direct_ : 1 {};  // BK72xx: mode, level and reads go through the BDK, see bk_set_mode()
  gpio::Flags flags_{};
};

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
