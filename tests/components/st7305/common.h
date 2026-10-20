#pragma once

#include <cstdio>
#include <utility>
#include <vector>

#include "esphome/components/spi/spi.h"
#include "esphome/components/st7305/st7305.h"
#include "esphome/core/hal.h"

namespace esphome::st7305::testing {

class TestableST7305 : public ST7305 {
 public:
  TestableST7305(int width = 400, int height = 300, Orientation orientation = Orientation::ORIENTATION_LANDSCAPE,
                 uint8_t column_start = 0x12, uint8_t column_end = 0x2A, uint8_t row_start = 0x00,
                 uint8_t row_end = 0xC7)
      : ST7305("TEST", width, height, orientation, column_start, column_end, row_start, row_end, nullptr, 0) {}

  bool init_buffer() { return this->init_buffer_(); }
  void clear_dirty() { this->clear_dirty_(); }
  bool is_dirty() const { return this->is_dirty_(); }
  int x_low() const { return this->x_low_; }
  int x_high() const { return this->x_high_; }
  int y_low() const { return this->y_low_; }
  int y_high() const { return this->y_high_; }
  Window compute_window() const { return this->compute_window_(); }
  void write_window() { this->write_window_(this->compute_window_()); }
  uint8_t *buffer() { return this->buffer_; }
  void attach_spi(spi::SPIDelegate *delegate, GPIOPin *dc_pin) {
    this->delegate_ = delegate;
    this->set_dc_pin(dc_pin);
  }

  size_t buffer_length() const { return this->buffer_length_; }
  int width() const { return this->width_; }
  int height() const { return this->height_; }
};

/// GPIO pin that remembers the last level written to it.
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

/// SPI delegate that records every payload and whether it was a command or data phase.
class RecordingDelegate : public spi::SPIDelegate {
 public:
  explicit RecordingDelegate(const RecordingPin *dc)
      : spi::SPIDelegate(10'000'000, spi::BIT_ORDER_MSB_FIRST, spi::MODE0, nullptr), dc_(dc) {}

  uint8_t transfer(uint8_t data) override {
    this->record_(&data, 1);
    return 0;
  }
  void write_array(const uint8_t *ptr, size_t length) override { this->record_(ptr, length); }

  std::vector<uint8_t> bytes;
  std::vector<std::pair<bool, size_t>> chunks;  // (data phase, length)

 protected:
  void record_(const uint8_t *ptr, size_t length) {
    this->chunks.emplace_back(this->dc_->level, length);
    this->bytes.insert(this->bytes.end(), ptr, ptr + length);
  }

  const RecordingPin *dc_;
};

}  // namespace esphome::st7305::testing
