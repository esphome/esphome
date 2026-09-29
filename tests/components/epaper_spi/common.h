#pragma once

#include <gtest/gtest.h>

#include <map>
#include <vector>

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

/// SPI delegate that records what reaches the bus, filing each payload under the command it
/// followed; commands are the bytes written while D/C is low. Optionally burns wall-clock time on
/// each data row, so a transfer can be driven past its yield deadline.
class RecordingDelegate : public spi::SPIDelegate {
 public:
  explicit RecordingDelegate(const RecordingPin *dc, uint32_t row_transfer_ms = 0)
      : dc_(dc), row_transfer_ms_(row_transfer_ms) {}

  uint8_t transfer(uint8_t data) override {
    this->record_(&data, 1);
    return 0;
  }
  void write_array(const uint8_t *ptr, size_t length) override {
    this->record_(ptr, length);
    if (this->dc_->level && this->row_transfer_ms_ != 0) {
      const uint32_t until = millis() + this->row_transfer_ms_;
      while (millis() < until) {
      }
    }
  }

  void clear() {
    this->commands.clear();
    this->data.clear();
  }

  std::vector<uint8_t> commands;
  std::map<uint8_t, std::vector<uint8_t>> data;

 protected:
  void record_(const uint8_t *ptr, size_t length) {
    if (!this->dc_->level) {
      for (size_t i = 0; i != length; i++) {
        this->commands.push_back(ptr[i]);
        this->last_command_ = ptr[i];
      }
      return;
    }
    auto &payload = this->data[this->last_command_];
    payload.insert(payload.end(), ptr, ptr + length);
  }

  const RecordingPin *dc_;
  uint32_t row_transfer_ms_;
  uint8_t last_command_{0};
};

}  // namespace esphome::epaper_spi::testing
