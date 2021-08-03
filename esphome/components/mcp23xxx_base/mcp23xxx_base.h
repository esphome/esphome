#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace mcp23xxx_base {

enum MCP23XXXInterruptMode : uint8_t { MCP23XXX_NO_INTERRUPT = 0, MCP23XXX_CHANGE, MCP23XXX_RISING, MCP23XXX_FALLING };

/// Modes for MCP23XXX pins
enum MCP23XXXGPIOMode : uint8_t {
  MCP23XXX_INPUT = INPUT,                // 0x00
  MCP23XXX_INPUT_PULLUP = INPUT_PULLUP,  // 0x02
  MCP23XXX_OUTPUT = OUTPUT               // 0x01
};

class MCP23XXXBase : public Component {
 public:
  virtual bool digital_read(uint8_t pin);
  virtual void digital_write(uint8_t pin, bool value);
  virtual void pin_mode(uint8_t pin, uint8_t mode);
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
  MCP23XXXGPIOPin(MCP23XXXBase *parent, uint8_t pin, uint8_t mode, bool inverted = false,
                  MCP23XXXInterruptMode interrupt_mode = MCP23XXX_NO_INTERRUPT);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  MCP23XXXBase *parent_;
  MCP23XXXInterruptMode interrupt_mode_;
};

}  // namespace mcp23xxx_base
}  // namespace esphome
