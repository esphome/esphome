#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp23016 {

enum MCP23016GPIORegisters {
  // 0 side
  MCP23016_GP0 = 0x00,
  MCP23016_OLAT0 = 0x02,
  MCP23016_IPOL0 = 0x04,
  MCP23016_IODIR0 = 0x06,
  MCP23016_INTCAP0 = 0x08,
  MCP23016_IOCON0 = 0x0A,
  // 1 side
  MCP23016_GP1 = 0x01,
  MCP23016_OLAT1 = 0x03,
  MCP23016_IPOL1 = 0x04,
  MCP23016_IODIR1 = 0x07,
  MCP23016_INTCAP1 = 0x08,
  MCP23016_IOCON1 = 0x0B,
};

class MCP23016 : public Component, public i2c::I2CDevice {
 public:
  MCP23016() = default;

  void setup() override;

  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

 protected:
  // read a given register
  bool read_reg_(uint8_t reg, uint8_t *value);
  // write a value to a given register
  bool write_reg_(uint8_t reg, uint8_t value);
  // update registers with given pin value.
  void update_reg_(uint8_t pin, bool pin_value, uint8_t reg_a);

  uint8_t olat_0_{0x00};
  uint8_t olat_1_{0x00};
};

class MCP23016GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(MCP23016 *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

 protected:
  MCP23016 *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace mcp23016
}  // namespace esphome
