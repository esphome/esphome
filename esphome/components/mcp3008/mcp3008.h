#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp3008 {

class MCP3008 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_75KHZ> {  // Running at the slowest max speed supported by the
                                                               // mcp3008. 2.7v = 75ksps
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float read_data(uint8_t pin);
};

}  // namespace mcp3008
}  // namespace esphome
