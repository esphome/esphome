#pragma once

#include "esphome/components/spa06_base/spa06_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::spa06_spi {

class SPA06SPIComponent : public spa06_base::SPA06Component,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_200KHZ> {
  void setup() override;
  bool spa_read_byte(uint8_t a_register, uint8_t *data) override;
  bool spa_write_byte(uint8_t a_register, uint8_t data) override;
  bool spa_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool spa_write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  void dump_config() override;

 protected:
  void protocol_reset() override;
};

}  // namespace esphome::spa06_spi
