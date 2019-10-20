#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp23008 {

/// Modes for MCP23008 pins
enum MCP23008GPIOMode : uint8_t {
  MCP23008_INPUT = INPUT,                // 0x00
  MCP23008_INPUT_PULLUP = INPUT_PULLUP,  // 0x02
  MCP23008_OUTPUT = OUTPUT               // 0x01
};

enum MCP23008GPIORegisters {
  // A side
  MCP23008_IODIR = 0x00,
  MCP23008_IPOL = 0x01,
  MCP23008_GPINTEN = 0x02,
  MCP23008_DEFVAL = 0x03,
  MCP23008_INTCON = 0x04,
  MCP23008_IOCON = 0x05,
  MCP23008_GPPU = 0x06,
  MCP23008_INTF = 0x07,
  MCP23008_INTCAP = 0x08,
  MCP23008_GPIO = 0x09,
  MCP23008_OLAT = 0x0A,
};

class MCP23008 : public Component, public i2c::I2CDevice {
 public:
  MCP23008() = default;

  void setup() override;

  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, uint8_t mode);

  float get_setup_priority() const override;

 protected:
  // read a given register
  bool read_reg_(uint8_t reg, uint8_t *value);
  // write a value to a given register
  bool write_reg_(uint8_t reg, uint8_t value);
  // update registers with given pin value.
  void update_reg_(uint8_t pin, bool pin_value, uint8_t reg_a);

  uint8_t olat_{0x00};
};

class MCP23008GPIOPin : public GPIOPin {
 public:
  MCP23008GPIOPin(MCP23008 *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  MCP23008 *parent_;
};

}  // namespace mcp23008
}  // namespace esphome
