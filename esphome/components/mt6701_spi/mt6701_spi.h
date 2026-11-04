#pragma once

#include "esphome/components/mt6701/mt6701.h"
#include "esphome/components/spi/spi.h"

namespace esphome::mt6701_spi {

/// Decode a 24-bit SSI frame (14-bit angle, 4 status bits, 6-bit CRC); false when the CRC fails
bool decode_frame(const uint8_t frame[3], uint16_t &count, uint8_t &status);

// Noise passes the 6-bit CRC 1 time in 64, so setup wants several good frames
static constexpr uint8_t PROBE_FRAMES = 10;
static constexpr uint8_t PROBE_MIN_VALID = 3;

enum class ProbeResult : uint8_t {
  PROBE_RESULT_NO_ENCODER,
  PROBE_RESULT_ALL_ZERO,  // a stuck-low data line, or an encoder at rest
  PROBE_RESULT_OK,
};
ProbeResult probe_result(uint8_t valid_frames, bool any_nonzero);

/// MT6701 over SSI: read-only, but each frame carries the field, push button and track-loss flags.
/// Data shifts out on the rising clock edge, so it is sampled on the falling edge with the clock
/// idle low; a clock idling high reads an extra leading bit.
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
