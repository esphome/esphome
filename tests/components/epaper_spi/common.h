#pragma once

#include <gtest/gtest.h>

#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"

namespace esphome::epaper_spi::testing {

/// SPI delegate that records transaction boundaries and burns wall-clock time on each
/// row write, so a transfer can be driven past its MAX_TRANSFER_TIME yield deadline.
class TimedSPIDelegate : public spi::SPIDelegate {
 public:
  explicit TimedSPIDelegate(uint32_t row_transfer_ms) : row_transfer_ms_(row_transfer_ms) {}

  uint8_t transfer(uint8_t data) override { return 0; }

  void write_array(const uint8_t *ptr, size_t length) override {
    // A row of pixel data is one "slow" write; single-byte writes are commands.
    if (length > 1) {
      const uint32_t until = millis() + this->row_transfer_ms_;
      while (millis() < until) {
      }
    }
  }

  void begin_transaction() override { this->begin_count++; }
  void end_transaction() override { this->end_count++; }

  int begin_count{0};
  int end_count{0};

 protected:
  uint32_t row_transfer_ms_;
};

/// GPIO pin that just remembers the last level written to it.
class RecordingPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::Flags::FLAG_NONE; }
  bool digital_read() override { return false; }
  void digital_write(bool value) override { this->level = value; }
  size_t dump_summary(char *buffer, size_t len) const override { return snprintf(buffer, len, "recording"); }

  bool level{true};
};

}  // namespace esphome::epaper_spi::testing
