#include "mcp23x17_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23x17_base {

static const char *const TAG = "mcp23x17_base";

bool MCP23X17Base::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? mcp23x17_base::MCP23X17_GPIOA : mcp23x17_base::MCP23X17_GPIOB;
  uint8_t value = 0;
  this->read_reg(reg_addr, &value);
  return value & (1 << bit);
}

void MCP23X17Base::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? mcp23x17_base::MCP23X17_OLATA : mcp23x17_base::MCP23X17_OLATB;
  this->update_reg(pin, value, reg_addr);
}

void MCP23X17Base::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t iodir = pin < 8 ? mcp23x17_base::MCP23X17_IODIRA : mcp23x17_base::MCP23X17_IODIRB;
  uint8_t gppu = pin < 8 ? mcp23x17_base::MCP23X17_GPPUA : mcp23x17_base::MCP23X17_GPPUB;
  if (flags == gpio::FLAG_INPUT) {
    this->update_reg(pin, true, iodir);
    this->update_reg(pin, false, gppu);
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    this->update_reg(pin, true, iodir);
    this->update_reg(pin, true, gppu);
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_reg(pin, false, iodir);
  }
}

void MCP23X17Base::pin_interrupt_mode(uint8_t pin, mcp23xxx_base::MCP23XXXInterruptMode interrupt_mode) {
  uint8_t gpinten = pin < 8 ? mcp23x17_base::MCP23X17_GPINTENA : mcp23x17_base::MCP23X17_GPINTENB;
  uint8_t intcon = pin < 8 ? mcp23x17_base::MCP23X17_INTCONA : mcp23x17_base::MCP23X17_INTCONB;
  uint8_t defval = pin < 8 ? mcp23x17_base::MCP23X17_DEFVALA : mcp23x17_base::MCP23X17_DEFVALB;

  switch (interrupt_mode) {
    case mcp23xxx_base::MCP23XXX_CHANGE:
      this->update_reg(pin, true, gpinten);
      this->update_reg(pin, false, intcon);
      break;
    case mcp23xxx_base::MCP23XXX_RISING:
      this->update_reg(pin, true, gpinten);
      this->update_reg(pin, true, intcon);
      this->update_reg(pin, true, defval);
      break;
    case mcp23xxx_base::MCP23XXX_FALLING:
      this->update_reg(pin, true, gpinten);
      this->update_reg(pin, true, intcon);
      this->update_reg(pin, false, defval);
      break;
    case mcp23xxx_base::MCP23XXX_NO_INTERRUPT:
      this->update_reg(pin, false, gpinten);
      break;
  }
}

void MCP23X17Base::update_reg(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == mcp23x17_base::MCP23X17_OLATA) {
    reg_value = this->olat_a_;
  } else if (reg_addr == mcp23x17_base::MCP23X17_OLATB) {
    reg_value = this->olat_b_;
  } else {
    this->read_reg(reg_addr, &reg_value);
  }

  if (pin_value) {
    reg_value |= 1 << bit;
  } else {
    reg_value &= ~(1 << bit);
  }

  this->write_reg(reg_addr, reg_value);

  if (reg_addr == mcp23x17_base::MCP23X17_OLATA) {
    this->olat_a_ = reg_value;
  } else if (reg_addr == mcp23x17_base::MCP23X17_OLATB) {
    this->olat_b_ = reg_value;
  }
}

}  // namespace mcp23x17_base
}  // namespace esphome
