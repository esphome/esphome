#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::mcp23016 {

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
  MCP23016_IPOL1 = 0x05,
  MCP23016_IODIR1 = 0x07,
  MCP23016_INTCAP1 = 0x09,
  MCP23016_IOCON1 = 0x0B,
};

class MCP23016 final : public gpio_expander::CachedGpioExpanderComponent<uint16_t, 16>, public i2c::I2CDevice {
 public:
  MCP23016() = default;

  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

 protected:
  // Virtual methods from CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  // read a given register
  bool read_reg_(uint8_t reg, uint16_t *value);
  // write a value to a given register
  bool write_reg_(uint8_t reg, uint16_t value);
  // update registers with given pin value.
  void update_reg_(uint8_t pin, bool pin_value, uint8_t reg_a);

  uint16_t olat_{0x0000};
};

using MCP23016GPIOPin = gpio_expander::ExpanderGPIOPin<MCP23016>;

}  // namespace esphome::mcp23016

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::mcp23016::MCP23016);
