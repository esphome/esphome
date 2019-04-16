#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp23017 {

/// Modes for MCP23017 pins
enum MCP23017GPIOMode : uint8_t {
  MCP23017_INPUT = INPUT,                // 0x00
  MCP23017_INPUT_PULLUP = INPUT_PULLUP,  // 0x02
  MCP23017_OUTPUT = OUTPUT               // 0x01
};

enum MCP23017GPIORegisters {
  // A side
  MCP23017_IODIRA = 0x00,
  MCP23017_IPOLA = 0x02,
  MCP23017_GPINTENA = 0x04,
  MCP23017_DEFVALA = 0x06,
  MCP23017_INTCONA = 0x08,
  MCP23017_IOCONA = 0x0A,
  MCP23017_GPPUA = 0x0C,
  MCP23017_INTFA = 0x0E,
  MCP23017_INTCAPA = 0x10,
  MCP23017_GPIOA = 0x12,
  MCP23017_OLATA = 0x14,
  // B side
  MCP23017_IODIRB = 0x01,
  MCP23017_IPOLB = 0x03,
  MCP23017_GPINTENB = 0x05,
  MCP23017_DEFVALB = 0x07,
  MCP23017_INTCONB = 0x09,
  MCP23017_IOCONB = 0x0B,
  MCP23017_GPPUB = 0x0D,
  MCP23017_INTFB = 0x0F,
  MCP23017_INTCAPB = 0x11,
  MCP23017_GPIOB = 0x13,
  MCP23017_OLATB = 0x15,
};

class MCP23017 : public Component, public i2c::I2CDevice {
 public:
  MCP23017() = default;

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

  uint8_t olat_a_{0x00};
  uint8_t olat_b_{0x00};
};

class MCP23017GPIOPin : public GPIOPin {
 public:
  MCP23017GPIOPin(MCP23017 *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  MCP23017 *parent_;
};

}  // namespace mcp23017
}  // namespace esphome
