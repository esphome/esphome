#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp23xxx_base {

enum MCP23XXXInterruptMode : uint8_t { MCP23XXX_NO_INTERRUPT = 0, MCP23XXX_CHANGE, MCP23XXX_RISING, MCP23XXX_FALLING };

class MCP23XXXBase : public Component {
 public:
  virtual bool digital_read(uint8_t pin);
  virtual void digital_write(uint8_t pin, bool value);
  virtual void pin_mode(uint8_t pin, gpio::Flags flags);
  virtual void pin_interrupt_mode(uint8_t pin, MCP23XXXInterruptMode interrupt_mode);

  void set_open_drain_ints(const bool value) { this->open_drain_ints_ = value; }
  float get_setup_priority() const override;

 protected:
  // read a given register
  virtual bool read_reg(uint8_t reg, uint8_t *value);
  // write a value to a given register
  virtual bool write_reg(uint8_t reg, uint8_t value);
  // update registers with given pin value.
  virtual void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a);

  bool open_drain_ints_;
};

class MCP23XXXGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(MCP23XXXBase *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }
  void set_interrupt_mode(MCP23XXXInterruptMode interrupt_mode) { interrupt_mode_ = interrupt_mode; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  MCP23XXXBase *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
  MCP23XXXInterruptMode interrupt_mode_;
};

}  // namespace mcp23xxx_base
}  // namespace esphome
