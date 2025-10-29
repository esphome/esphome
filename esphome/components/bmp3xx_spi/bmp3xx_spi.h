#pragma once
#include "esphome/components/bmp3xx_base/bmp3xx_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace bmp3xx_spi {

class BMP3XXSPIComponent : public bmp3xx_base::BMP3XXComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
  void setup() override;
  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
};

}  // namespace bmp3xx_spi
}  // namespace esphome
