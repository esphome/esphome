#include "mcp23x17_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23x17_base {

static const char *const TAG = "mcp23x17_base";

bool MCP23X17Base::digital_read_hw(uint8_t pin) {
  uint8_t data;
  if (pin < 8) {
    if (!this->read_reg(mcp23x17_base::MCP23X17_GPIOA, &data)) {
      this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return false;
    }
    this->input_mask_ = encode_uint16(this->input_mask_ >> 8, data);
  } else {
    if (!this->read_reg(mcp23x17_base::MCP23X17_GPIOB, &data)) {
      this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return false;
    }
    this->input_mask_ = encode_uint16(data, this->input_mask_ & 0xFF);
  }
  return true;
}

void MCP23X17Base::digital_write_hw(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? mcp23x17_base::MCP23X17_OLATA : mcp23x17_base::MCP23X17_OLATB;
  this->update_reg(pin, value, reg_addr);
}

bool MCP23X17Base::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }

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

optional<uint8_t> MCP23X17Base::read_interrupt_status_(uint8_t bank) {
  uint8_t intf_reg = bank == 0 ? mcp23x17_base::MCP23X17_INTFA : mcp23x17_base::MCP23X17_INTFB;
  uint8_t intcap_reg = bank == 0 ? mcp23x17_base::MCP23X17_INTCAPA : mcp23x17_base::MCP23X17_INTCAPB;

  // Read interrupt flag register
  uint8_t intf = 0;
  if (!this->read_reg(intf_reg, &intf)) {
    ESP_LOGW(TAG, "Failed to read interrupt flags for bank %u", bank);
    return nullopt;
  }

  // If no interrupts, return early
  if (intf == 0) {
    return 0;
  }

  // Read interrupt capture register (pin values at time of interrupt)
  uint8_t intcap = 0;
  if (!this->read_reg(intcap_reg, &intcap)) {
    ESP_LOGW(TAG, "Failed to read interrupt capture for bank %u", bank);
    return nullopt;
  }

  // Update the input_mask_ with captured values
  if (bank == 0) {
    this->input_mask_ = encode_uint16(this->input_mask_ >> 8, intcap);
  } else {
    this->input_mask_ = encode_uint16(intcap, this->input_mask_ & 0xFF);
  }

  ESP_LOGV(TAG, "Interrupt on bank %u: flags=0x%02X, captured=0x%02X", bank, intf, intcap);
  return intf;
}

}  // namespace mcp23x17_base
}  // namespace esphome
