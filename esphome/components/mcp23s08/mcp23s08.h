#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp23s08 {

/// Modes for MCP23S08 pins
enum MCP23S08GPIOMode : uint8_t {
  MCP23S08_INPUT = INPUT,                // 0x00
  MCP23S08_INPUT_PULLUP = INPUT_PULLUP,  // 0x02
  MCP23S08_OUTPUT = OUTPUT               // 0x01
};

enum MCP23S08GPIORegisters {
  // A side
  MCP23S08_IODIR = 0x00,
  MCP23S08_IPOL = 0x01,
  MCP23S08_GPINTEN = 0x02,
  MCP23S08_DEFVAL = 0x03,
  MCP23S08_INTCON = 0x04,
  MCP23S08_IOCON = 0x05,
  MCP23S08_GPPU = 0x06,
  MCP23S08_INTF = 0x07,
  MCP23S08_INTCAP = 0x08,
  MCP23S08_GPIO = 0x09,
  MCP23S08_OLAT = 0x0A,
};

class MCP23S08 : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_10MHZ> {
 public:
  MCP23S08() = default;

  void setup() override;
  void dump_config() override;
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, uint8_t mode);

  void set_device_address(uint8_t device_addr);

  float get_setup_priority() const override;

  // read a given register
  bool read_reg(uint8_t reg, uint8_t *value);
  // write a value to a given register
  bool write_reg(uint8_t reg, uint8_t value);
  // update registers with given pin value.
  void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a);

 protected:
  uint8_t device_opcode_ = 0x40;
  uint8_t olat_{0x00};
};

class MCP23S08GPIOPin : public GPIOPin {
 public:
  MCP23S08GPIOPin(MCP23S08 *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  MCP23S08 *parent_;
};

}  // namespace mcp23s08
}  // namespace esphome
