#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp23xxx_base {

enum MCP23XXXInterruptMode : uint8_t { MCP23XXX_NO_INTERRUPT = 0, MCP23XXX_CHANGE, MCP23XXX_RISING, MCP23XXX_FALLING };

template<uint8_t N> class MCP23XXXBase : public Component, public gpio_expander::CachedGpioExpander<uint8_t, N> {
 public:
  virtual void pin_mode(uint8_t pin, gpio::Flags flags);
  virtual void pin_interrupt_mode(uint8_t pin, MCP23XXXInterruptMode interrupt_mode);

  void set_open_drain_ints(const bool value) { this->open_drain_ints_ = value; }
  float get_setup_priority() const override { return setup_priority::IO; }

  void loop() override { this->reset_pin_cache_(); }

 protected:
  // read a given register
  virtual bool read_reg(uint8_t reg, uint8_t *value) = 0;
  // write a value to a given register
  virtual bool write_reg(uint8_t reg, uint8_t value) = 0;
  // update registers with given pin value.
  virtual void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a) = 0;

  bool open_drain_ints_;
};

template<uint8_t N> class MCP23XXXGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

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

}  // namespace mcp23xxx_base
}  // namespace esphome
