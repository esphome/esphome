#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23x08_base/mcp23x08_base.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp23s08 {

class MCP23S08 : public mcp23x08_base::MCP23X08Base,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_10MHZ> {
 public:
  MCP23S08() = default;

  void setup() override;
  void dump_config() override;

  void set_device_address(uint8_t device_addr);

 protected:
  bool read_reg(uint8_t reg, uint8_t *value) override;
  bool write_reg(uint8_t reg, uint8_t value) override;

  uint8_t device_opcode_ = 0x40;
};

}  // namespace mcp23s08
}  // namespace esphome
