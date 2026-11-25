#pragma once

#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome::gpio_expander {

/// Pin of a GPIO expander. Parent provides pin_mode(pin, flags), digital_read(pin) and digital_write(pin, value).
/// dump_summary() is specialized per expander with the macros below, outside the template so that its format
/// string stays in flash on ESP8266; a missing macro is an undefined reference to
/// ExpanderGPIOPin<X>::dump_summary. Built by pins.new_parented_pin().
template<typename Parent> class ExpanderGPIOPin final : public GPIOPin, public Parented<Parent> {
 public:
  // User provided, not "= default": `new(p) ExpanderGPIOPin()` would zero-fill .bss that is already zero.
  ExpanderGPIOPin() {}

  void setup() override { this->pin_mode(this->flags_); }
  void pin_mode(gpio::Flags flags) override { this->parent_->pin_mode(this->pin_, flags); }
  bool digital_read() override { return this->parent_->digital_read(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->digital_write(this->pin_, value != this->inverted_); }
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{};
};

}  // namespace esphome::gpio_expander

/// In the expander's header, at file scope: declares the pin summary its .cpp defines.
#define ESPHOME_EXPANDER_PIN_SUMMARY_DECL(Parent) \
  template<> size_t esphome::gpio_expander::ExpanderGPIOPin<Parent>::dump_summary(char *buffer, size_t len) const

/// In the expander's .cpp, at file scope: the pin summary, a printf format taking the pin number.
#define ESPHOME_EXPANDER_PIN_SUMMARY(Parent, format) \
  ESPHOME_EXPANDER_PIN_SUMMARY_DECL(Parent) { return buf_append_printf(buffer, len, 0, format, this->pin_); }
