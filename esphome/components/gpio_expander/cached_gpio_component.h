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
  /// Call at the end of setup(): attach the falling-edge ISR when there is an interrupt pin, then disable the
  /// loop until an input needs it.
  void setup_interrupt_pin_();
  /// The loop runs again after the INT line falls; stop it once the line is released (HIGH).
  /// Only called with an interrupt pin.
  void disable_loop_when_released_();
  /// Call from pin_mode() for an input pin: without an interrupt pin the loop polls instead.
  void enable_polling_loop_() {
    if (this->interrupt_pin_ == nullptr)
      this->enable_loop();
  }

  InternalGPIOPin *interrupt_pin_{nullptr};
};

/// Base for expanders that cache reads per loop, with the per-bank input store of GpioExpanderInputs. Without
/// an interrupt pin each read invalidates its pin; with one the cache stays valid until INT fires.
template<typename T, uint16_t N>
class CachedGpioExpanderComponent : public InterruptExpanderComponent,
                                    public GpioExpanderInputs<CachedGpioExpanderBase<T, N>> {
 public:
  bool digital_read(expander_pin_t<N> pin) { return this->cached_read_(pin, this->interrupt_pin_ == nullptr); }

  void loop() override {
    this->reset_pin_cache_();
    if (this->interrupt_pin_ != nullptr)
      this->disable_loop_when_released_();
  }
};

}  // namespace esphome::gpio_expander
