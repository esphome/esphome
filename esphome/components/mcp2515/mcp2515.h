#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace mcp2515 {

class MCP2515 : public canbus::Canbus,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_8MHZ> {
 public:
  MCP2515(){};
  MCP2515(const std::string &name){};

  void set_cs_pin(GPIOPin *cs_pin) { cs_pin_ = cs_pin; }

 protected:
  GPIOPin *cs_pin_;
  bool send_internal_(int can_id, uint8_t *data);
};
}  // namespace mcp2515
}  // namespace esphome