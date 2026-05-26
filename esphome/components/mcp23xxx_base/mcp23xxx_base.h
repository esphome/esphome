#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::mcp23xxx_base {

enum MCP23XXXInterruptMode : uint8_t { MCP23XXX_NO_INTERRUPT = 0, MCP23XXX_CHANGE, MCP23XXX_RISING, MCP23XXX_FALLING };

template<uint8_t N> class MCP23XXXBase : public Component, public gpio_expander::CachedGpioExpander<uint8_t, N> {
 public:
  virtual void pin_mode(uint8_t pin, gpio::Flags flags);
  virtual void pin_interrupt_mode(uint8_t pin, MCP23XXXInterruptMode interrupt_mode);

  void set_open_drain_ints(const bool value) { this->open_drain_ints_ = value; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  InternalGPIOPin *get_interrupt_pin() const { return this->interrupt_pin_; }
  float get_setup_priority() const override { return setup_priority::IO; }

  void loop() override {
    this->reset_pin_cache_();
    // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
    // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
    // stale state forever; keep looping until the line is released so we self-heal.
    if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
      this->disable_loop();
    }
  }

 protected:
  // No need to clear latched interrupts before attaching the ISR — if INT is
  // already low the ISR fires immediately, loop runs, cache invalidates, and
  // the GPIO read clears the latch. One harmless extra read at most.
  void setup_interrupt_pin_() {
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->setup();
      this->interrupt_pin_->attach_interrupt(&MCP23XXXBase::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
      this->set_invalidate_on_read_(false);
    }
    // Disable loop until an input pin is configured via pin_mode()
    // For interrupt-driven mode, loop is re-enabled by the ISR
    // For polling mode, loop is re-enabled when pin_mode() registers an input pin
    this->disable_loop();
  }
  static void IRAM_ATTR gpio_intr(MCP23XXXBase *arg) { arg->enable_loop_soon_any_context(); }

  // read a given register
  virtual bool read_reg(uint8_t reg, uint8_t *value) = 0;
  // write a value to a given register
  virtual bool write_reg(uint8_t reg, uint8_t value) = 0;
  // update registers with given pin value.
  virtual void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a) = 0;

  bool open_drain_ints_;
  InternalGPIOPin *interrupt_pin_{nullptr};
};

template<uint8_t N> class MCP23XXXGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_parent(MCP23XXXBase<N> *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }
  void set_interrupt_mode(MCP23XXXInterruptMode interrupt_mode) { interrupt_mode_ = interrupt_mode; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  MCP23XXXBase<N> *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
  MCP23XXXInterruptMode interrupt_mode_;
};

}  // namespace esphome::mcp23xxx_base
