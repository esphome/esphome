#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/st25r/st25r.h"

namespace esphome::st25r_spi {

class ST25RSpi : public st25r::ST25R,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                       spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  uint8_t read_register(uint8_t reg) override;
  void write_register(uint8_t reg, uint8_t value) override;
  void write_command(uint8_t command) override;
  void write_fifo(const uint8_t *data, size_t len) override;
  void read_fifo(uint8_t *data, size_t len) override;
};

}  // namespace esphome::st25r_spi
