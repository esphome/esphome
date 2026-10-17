#pragma once

#include "esphome/components/mt6701/mt6701.h"
#include "esphome/components/spi/spi.h"

namespace esphome::mt6701_spi {

/// Decode a 24-bit SSI frame: a 14-bit angle, 4 status bits and a 6-bit CRC (x^6 + x + 1) over
/// the 18 bits before it, most significant bit first. Returns false when the CRC does not match.
bool decode_frame(const uint8_t frame[3], uint16_t &count, uint8_t &status);

// Noise passes the 6-bit CRC 1 time in 64, so a present encoder must send PROBE_MIN_VALID good
// frames out of PROBE_FRAMES at setup
static constexpr uint8_t PROBE_FRAMES = 10;
static constexpr uint8_t PROBE_MIN_VALID = 3;

enum class ProbeResult : uint8_t {
  PROBE_RESULT_NO_ENCODER,  // too few frames passed the CRC
  PROBE_RESULT_ALL_ZERO,    // every good frame was zero: a stuck-low data line, or a real encoder at rest
  PROBE_RESULT_OK,
};
ProbeResult probe_result(uint8_t valid_frames, bool any_nonzero);

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
