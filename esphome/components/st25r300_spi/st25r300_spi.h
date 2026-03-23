#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/st25r300/st25r300.h"

namespace esphome {
namespace st25r300_spi {

// ST25R300 SPI protocol (Table 5, DS):
//   Register write: addr & 0x7F (bit7=0), then data
//   Register read:  addr | 0x80 (bit7=1), then read byte
//   FIFO write:     0x5F, then data bytes
//   FIFO read:      0xDF (0x80|0x5F), then read bytes
//   Direct command: send command byte directly (0x60-0x7F or 0xE2-0xEF range)
// SPI Mode 1: CPOL=0, CPHA=1 (CLOCK_PHASE_TRAILING) — same as ST25R3916
class ST25R300Spi : public st25r300::ST25R300,
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

}  // namespace st25r300_spi
}  // namespace esphome
