#include "cached_gpio_component.h"

namespace esphome::gpio_expander {

// Not a member of the template: GCC 10 (ESP8266) ignores section attributes on template instantiations.
static void IRAM_ATTR interrupt_pin_isr(Component *arg) { arg->enable_loop_soon_any_context(); }

// Like the per-chip code this replaces, an INT line already low at attach gives no falling edge, so
// nothing runs until the next edge.
bool InterruptExpanderComponent::attach_interrupt_pin_() {
  // Polling mode enables the loop in pin_mode() for the first input; interrupt mode from the ISR.
  this->disable_loop();
  if (this->interrupt_pin_ == nullptr)
    return false;
  this->interrupt_pin_->setup();
  this->interrupt_pin_->attach_interrupt(&interrupt_pin_isr, static_cast<Component *>(this),
                                         gpio::INTERRUPT_FALLING_EDGE);
  return true;
}

// Input changes that straddle the read leave INT asserted without a new falling edge, so keep looping
// until the line is released or the expander would keep stale state forever.
void InterruptExpanderComponent::disable_loop_when_released_() {
  if (this->interrupt_pin_->digital_read())
    this->disable_loop();
}

}  // namespace esphome::gpio_expander
