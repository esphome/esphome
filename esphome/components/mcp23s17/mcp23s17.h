#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23x17_base/mcp23x17_base.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp23s17 {

class MCP23S17 : public mcp23x17_base::MCP23X17Base,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_8MHZ> {
 public:
  MCP23S17() = default;

  void setup() override;
  void dump_config() override;
  void set_device_address(uint8_t device_addr);

 protected:
  bool read_reg(uint8_t reg, uint8_t *value) override;
  bool write_reg(uint8_t reg, uint8_t value) override;

  uint8_t device_opcode_ = 0x40;
};

}  // namespace mcp23s17
}  // namespace esphome
