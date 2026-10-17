#pragma once

#include "esphome/components/mt6701/mt6701.h"
#include "esphome/components/spi/spi.h"

namespace esphome::mt6701_spi {

/// MT6701 driver over the SSI (SPI-compatible) interface.
///
/// SSI is read-only: it cannot write configuration registers, but every frame
/// carries the magnetic-field status, push button and track-loss flags that the
/// I2C interface does not expose. Each 24-bit frame is CRC-checked.
///
/// The chip shifts data out on the rising clock edge, so it is sampled on the falling edge
/// with the clock idle low: the 24 bits then line up without the extra leading bit that a
/// clock idling high would read.
class MT6701SPIComponent final : public mt6701::MT6701Component,
                                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                       spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  /// The 4-bit status of the last good frame: bit3 track loss, bit2 push button, bits1:0 field strength
  uint8_t get_status() const { return this->status_; }

 protected:
  bool read_count(uint16_t &count) override;

  uint8_t status_{0};
};

}  // namespace esphome::mt6701_spi
