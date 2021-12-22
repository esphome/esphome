#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23xxx_base/mcp23xxx_base.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp23x17_base {

enum MCP23X17GPIORegisters {
  // A side
  MCP23X17_IODIRA = 0x00,
  MCP23X17_IPOLA = 0x02,
  MCP23X17_GPINTENA = 0x04,
  MCP23X17_DEFVALA = 0x06,
  MCP23X17_INTCONA = 0x08,
  MCP23X17_IOCONA = 0x0A,
  MCP23X17_GPPUA = 0x0C,
  MCP23X17_INTFA = 0x0E,
  MCP23X17_INTCAPA = 0x10,
  MCP23X17_GPIOA = 0x12,
  MCP23X17_OLATA = 0x14,
  // B side
  MCP23X17_IODIRB = 0x01,
  MCP23X17_IPOLB = 0x03,
  MCP23X17_GPINTENB = 0x05,
  MCP23X17_DEFVALB = 0x07,
  MCP23X17_INTCONB = 0x09,
  MCP23X17_IOCONB = 0x0B,
  MCP23X17_GPPUB = 0x0D,
  MCP23X17_INTFB = 0x0F,
  MCP23X17_INTCAPB = 0x11,
  MCP23X17_GPIOB = 0x13,
  MCP23X17_OLATB = 0x15,
};

class MCP23X17Base : public mcp23xxx_base::MCP23XXXBase {
 public:
  bool digital_read(uint8_t pin) override;
  void digital_write(uint8_t pin, bool value) override;
  void pin_mode(uint8_t pin, gpio::Flags flags) override;
  void pin_interrupt_mode(uint8_t pin, mcp23xxx_base::MCP23XXXInterruptMode interrupt_mode) override;

 protected:
  void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a) override;

  uint8_t olat_a_{0x00};
  uint8_t olat_b_{0x00};
};

}  // namespace mcp23x17_base
}  // namespace esphome
