#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "esphome/components/uart/uart_component.h"

namespace esphome::uart::benchmarks {

// Serves a fixed byte stream as received data; rewind() makes it available again.
class FakeUART : public UARTComponent {
 public:
  FakeUART(const uint8_t *data, size_t len) : data_(data), len_(len) {}

  void rewind() { this->pos_ = 0; }

  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override {
    if (this->pos_ >= this->len_)
      return false;
    *data = this->data_[this->pos_];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->len_ - this->pos_)
      return false;
    std::memcpy(data, this->data_ + this->pos_, len);
    this->pos_ += len;
    return true;
  }
  size_t available() override { return this->len_ - this->pos_; }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

 protected:
  const uint8_t *data_;
  size_t len_;
  size_t pos_{0};
};

}  // namespace esphome::uart::benchmarks
