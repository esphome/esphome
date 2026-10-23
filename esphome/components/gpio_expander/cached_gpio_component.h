#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::gpio_expander {

/// Component side of a cached GPIO expander with an optional active-low INT line. Not a template, so the
/// ISR and the shared code are emitted once for every expander.
class InterruptExpanderComponent : public Component {
 public:
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  float get_setup_priority() const override { return setup_priority::IO; }

 protected:
  /// Attach the falling-edge ISR when there is an interrupt pin, then disable the loop until an input
  /// needs it. Returns whether the interrupt pin is in use.
  bool attach_interrupt_pin_();
  /// The loop runs again after the INT line falls; stop it once the line is released (HIGH).
  void disable_loop_when_released_();
  /// Call from pin_mode() for an input pin: without an interrupt pin the loop polls instead.
  void enable_polling_loop_() {
    if (this->interrupt_pin_ == nullptr)
      this->enable_loop();
  }

  InternalGPIOPin *interrupt_pin_{nullptr};
};

/// Base for expanders that cache reads per loop. With an interrupt pin the cache stays valid until INT fires.
template<typename T, uint16_t N, typename P = typename std::conditional<(N > 256), uint16_t, uint8_t>::type>
class CachedGpioExpanderComponent : public InterruptExpanderComponent, public CachedGpioExpander<T, N, P> {
 public:
  void loop() override {
    this->reset_pin_cache_();
    this->disable_loop_when_released_();
  }

 protected:
  /// Call at the end of setup().
  void setup_interrupt_pin_() {
    if (this->attach_interrupt_pin_())
      this->set_invalidate_on_read_(false);
  }
};

}  // namespace esphome::gpio_expander
