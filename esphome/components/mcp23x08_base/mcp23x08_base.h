#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23xxx_base/mcp23xxx_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp23x08_base {

enum MCP23S08GPIORegisters {
  // A side
  MCP23X08_IODIR = 0x00,
  MCP23X08_IPOL = 0x01,
  MCP23X08_GPINTEN = 0x02,
  MCP23X08_DEFVAL = 0x03,
  MCP23X08_INTCON = 0x04,
  MCP23X08_IOCON = 0x05,
  MCP23X08_GPPU = 0x06,
  MCP23X08_INTF = 0x07,
  MCP23X08_INTCAP = 0x08,
  MCP23X08_GPIO = 0x09,
  MCP23X08_OLAT = 0x0A,
};

class MCP23X08Base : public mcp23xxx_base::MCP23XXXBase {
 public:
  bool digital_read(uint8_t pin) override;
  void digital_write(uint8_t pin, bool value) override;
  void pin_mode(uint8_t pin, gpio::Flags flags) override;
  void pin_interrupt_mode(uint8_t pin, mcp23xxx_base::MCP23XXXInterruptMode interrupt_mode) override;

 protected:
  void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a) override;

  uint8_t olat_{0x00};
};

}  // namespace mcp23x08_base
}  // namespace esphome
