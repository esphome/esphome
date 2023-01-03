#include "mcp23x08_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23x08_base {

static const char *const TAG = "mcp23x08_base";

bool MCP23X08Base::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = mcp23x08_base::MCP23X08_GPIO;
  uint8_t value = 0;
  this->read_reg(reg_addr, &value);
  return value & (1 << bit);
}

void MCP23X08Base::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = mcp23x08_base::MCP23X08_OLAT;
  this->update_reg(pin, value, reg_addr);
}

void MCP23X08Base::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t iodir = mcp23x08_base::MCP23X08_IODIR;
  uint8_t gppu = mcp23x08_base::MCP23X08_GPPU;
  if (flags == gpio::FLAG_INPUT) {
    this->update_reg(pin, true, iodir);
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    this->update_reg(pin, true, iodir);
    this->update_reg(pin, true, gppu);
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_reg(pin, false, iodir);
  }
}

void MCP23X08Base::pin_interrupt_mode(uint8_t pin, mcp23xxx_base::MCP23XXXInterruptMode interrupt_mode) {
  uint8_t gpinten = mcp23x08_base::MCP23X08_GPINTEN;
  uint8_t intcon = mcp23x08_base::MCP23X08_INTCON;
  uint8_t defval = mcp23x08_base::MCP23X08_DEFVAL;

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

void MCP23X08Base::update_reg(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == mcp23x08_base::MCP23X08_OLAT) {
    reg_value = this->olat_;
  } else {
    this->read_reg(reg_addr, &reg_value);
  }

  if (pin_value) {
    reg_value |= 1 << bit;
  } else {
    reg_value &= ~(1 << bit);
  }

  this->write_reg(reg_addr, reg_value);

  if (reg_addr == mcp23x08_base::MCP23X08_OLAT) {
    this->olat_ = reg_value;
  }
}

}  // namespace mcp23x08_base
}  // namespace esphome
