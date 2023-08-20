#pragma once

#include "esphome/core/component.h"
#include "esphome/components/hp303b/hp303b.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace hp303b_spi {
class HP303BComponentSPI : public hp303b::HP303BComponent,
                           public PollingComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;

  void dump_config() override;
  int16_t set_interrupt_polarity(uint8_t polarity) override;
  int16_t read_byte(uint8_t reg_address) override;
  int16_t read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer) override;
  int16_t write_byte(uint8_t reg_address, uint8_t data, uint8_t check) override;
  int16_t set_interrupt_polarity(uint8_t polarity) override;
};
}  // namespace hp303b_spi
}  // namespace esphome