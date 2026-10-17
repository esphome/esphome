#pragma once

#include "esphome/core/component.h"

namespace esphome::mt6701 {

// A 14-bit absolute angle: one revolution is 16384 counts
static constexpr float COUNTS_TO_DEGREES = 360.0f / 16384;
// Consecutive read or CRC failures tolerated before the component reports a warning
static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 5;

/// Bus-independent core of the MT6701 driver. The I2C and SSI/SPI components provide
/// the transport through read_count(); the entities read the encoder when they poll.
class MT6701Component : public Component {
 public:
  /// Read a fresh angle from the bus. Returns false on a bus or CRC error (the
  /// cached value is left unchanged), while the bus is suspended (EEPROM
  /// programming in flight), or when the hub failed setup.
  bool read_encoder();

  /// Most recent absolute angle as a raw count in the range [0, 16383].
  uint16_t get_count() const { return this->count_; }
  /// Most recent absolute shaft angle in degrees, in the range [0, 360).
  float get_angle_degrees() const { return this->count_ * COUNTS_TO_DEGREES; }

 protected:
  /// Read the current 14-bit count from the bus into count.
  /// Returns false on a bus or CRC error, in which case the sample is dropped.
  virtual bool read_count(uint16_t &count) = 0;

  uint16_t count_{0};
  uint8_t consecutive_errors_{0};
  // Set by the I2C hub while the EEPROM is programmed; no bus access until it clears
  bool suspend_sampling_{false};
};

}  // namespace esphome::mt6701
