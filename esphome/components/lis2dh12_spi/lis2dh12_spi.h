#pragma once

#include "esphome/components/lis2dh12_base/lis2dh12_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::lis2dh12_spi {

class LIS2DH12SPIComponent : public lis2dh12_base::LIS2DH12Component,
                             public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                   spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
};

}  // namespace esphome::lis2dh12_spi
