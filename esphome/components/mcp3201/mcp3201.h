#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp3201 {

class MCP3201 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  MCP3201() = default;

  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float read_data();

 protected:
  float reference_voltage_;
};

}  // namespace mcp3201
}  // namespace esphome
