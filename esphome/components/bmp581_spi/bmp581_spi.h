#pragma once

#include "esphome/components/bmp581_base/bmp581_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::bmp581_spi {

// BMP581 is technically compatible with SPI Mode0 and Mode3. Default to Mode3.
class BMP581SPIComponent : public esphome::bmp581_base::BMP581Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                 spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  bool bmp_read_byte(uint8_t a_register, uint8_t *data) override;
  bool bmp_write_byte(uint8_t a_register, uint8_t data) override;
  bool bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool bmp_write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  void dump_config() override;

 protected:
  void activate_interface() override;
};

}  // namespace esphome::bmp581_spi
