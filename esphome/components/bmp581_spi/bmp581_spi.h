#pragma once

#include "esphome/components/bmp581_base/bmp581_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::bmp581_spi {

class BMP581SPIComponent : public esphome::bmp581_base::BMP581Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
  void setup() override;
  bool bmp_read_byte(uint8_t a_register, uint8_t *data) override;
  bool bmp_write_byte(uint8_t a_register, uint8_t data) override;
  bool bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool bmp_write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  void activate_protocol_();
};

}  // namespace esphome::bmp581_spi
