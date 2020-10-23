#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp23s17 {

/// Modes for MCP23S17 pins
enum MCP23S17GPIOMode : uint8_t {
  MCP23S17_INPUT = INPUT,                // 0x00
  MCP23S17_INPUT_PULLUP = INPUT_PULLUP,  // 0x02
  MCP23S17_OUTPUT = OUTPUT               // 0x01
};

enum MCP23S17GPIORegisters {
  // A side
  MCP23S17_IODIRA = 0x00,
  MCP23S17_IPOLA = 0x02,
  MCP23S17_GPINTENA = 0x04,
  MCP23S17_DEFVALA = 0x06,
  MCP23S17_INTCONA = 0x08,
  MCP23S17_IOCONA = 0x0A,
  MCP23S17_GPPUA = 0x0C,
  MCP23S17_INTFA = 0x0E,
  MCP23S17_INTCAPA = 0x10,
  MCP23S17_GPIOA = 0x12,
  MCP23S17_OLATA = 0x14,
  // B side
  MCP23S17_IODIRB = 0x01,
  MCP23S17_IPOLB = 0x03,
  MCP23S17_GPINTENB = 0x05,
  MCP23S17_DEFVALB = 0x07,
  MCP23S17_INTCONB = 0x09,
  MCP23S17_IOCONB = 0x0B,
  MCP23S17_GPPUB = 0x0D,
  MCP23S17_INTFB = 0x0F,
  MCP23S17_INTCAPB = 0x11,
  MCP23S17_GPIOB = 0x13,
  MCP23S17_OLATB = 0x15,
};

class MCP23S17 : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_8MHZ> {
 public:
  MCP23S17() = default;

  void setup() override;
  void dump_config() override;
  void set_device_address(uint8_t device_addr);

  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, uint8_t mode);

  float get_setup_priority() const override;

  // read a given register
  bool read_reg(uint8_t reg, uint8_t *value);
  // write a value to a given register
  bool write_reg(uint8_t reg, uint8_t value);
  // update registers with given pin value.
  void update_reg(uint8_t pin, bool pin_value, uint8_t reg_a);

 protected:
  uint8_t device_opcode_ = 0x40;
  uint8_t olat_a_{0x00};
  uint8_t olat_b_{0x00};
};

class MCP23S17GPIOPin : public GPIOPin {
 public:
  MCP23S17GPIOPin(MCP23S17 *parent, uint8_t pin, uint8_t mode, bool inverted = false);

  void setup() override;
  void pin_mode(uint8_t mode) override;
  bool digital_read() override;
  void digital_write(bool value) override;

 protected:
  MCP23S17 *parent_;
};

}  // namespace mcp23s17
}  // namespace esphome
