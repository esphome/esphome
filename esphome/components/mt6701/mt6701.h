#pragma once

#include "esphome/core/component.h"

namespace esphome::mt6701 {

// The MT6701 is a 14-bit absolute magnetic angle encoder: one mechanical
// revolution maps to 16384 counts.
static const uint16_t COUNTS_PER_REV = 16384;
static const float COUNTS_TO_DEGREES = 360.0f / COUNTS_PER_REV;

// Number of consecutive read/CRC failures tolerated before the component
// reports a warning status.
static const uint8_t MAX_CONSECUTIVE_ERRORS = 5;

/// Bus-independent core of the MT6701 driver.
///
/// The I2C and SSI/SPI components provide the transport by implementing
/// read_count(). The hub polls the encoder on its update_interval, and the
/// sensor platform triggers an extra read whenever it polls so published angles
/// are always fresh.
class MT6701Component : public PollingComponent {
 public:
  void update() override { this->read_encoder(); }

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

  /// Account for a dropped sample and raise a warning after repeated failures.
  void handle_read_error_();

  uint16_t count_{0};
  uint8_t consecutive_errors_{0};
  // Gates all bus access; set by transports while the bus must stay untouched
  // (the I2C hub uses it during the EEPROM programming window).
  bool suspend_sampling_{false};
};

}  // namespace esphome::mt6701
