#include "mcp23x08_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23x08_base {

static const char *const TAG = "mcp23x08_base";

bool MCP23X08Base::digital_read_hw(uint8_t pin) {
  if (!this->read_reg(mcp23x08_base::MCP23X08_GPIO, &this->input_mask_)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  return true;
}

void MCP23X08Base::digital_write_hw(uint8_t pin, bool value) {
  uint8_t reg_addr = mcp23x08_base::MCP23X08_OLAT;
  this->update_reg(pin, value, reg_addr);
}

bool MCP23X08Base::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }

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

optional<uint8_t> MCP23X08Base::read_interrupt_status_(uint8_t bank) {
  // MCP23X08 only has one bank (bank 0)
  if (bank != 0) {
    return 0;
  }

  // Read interrupt flag register
  uint8_t intf = 0;
  if (!this->read_reg(mcp23x08_base::MCP23X08_INTF, &intf)) {
    ESP_LOGW(TAG, "Failed to read interrupt flags");
    return nullopt;
  }

  // If no interrupts, return early
  if (intf == 0) {
    return 0;
  }

  // Read interrupt capture register (pin values at time of interrupt)
  uint8_t intcap = 0;
  if (!this->read_reg(mcp23x08_base::MCP23X08_INTCAP, &intcap)) {
    ESP_LOGW(TAG, "Failed to read interrupt capture");
    return nullopt;
  }

  // Update the input_mask_ with captured values
  this->input_mask_ = intcap;

  ESP_LOGV(TAG, "Interrupt: flags=0x%02X, captured=0x%02X", intf, intcap);
  return intf;
}

}  // namespace mcp23x08_base
}  // namespace esphome
