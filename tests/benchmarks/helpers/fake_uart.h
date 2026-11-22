#pragma once

#include <cstdint>
#include <cstring>

#include "esphome/components/uart/uart_component.h"

namespace esphome::benchmarks {

// Serves a fixed byte stream as received data. Every write rewinds it, so the stream answers each request.
class FakeUART : public uart::UARTComponent {
 public:
  void load(const uint8_t *data, size_t len) {
    this->data_ = data;
    this->len_ = len;
    this->pos_ = 0;
  }
  void write_array(const uint8_t *data, size_t len) override { this->pos_ = 0; }
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
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

 protected:
  const uint8_t *data_{nullptr};
  size_t len_{0};
  size_t pos_{0};
};

}  // namespace esphome::benchmarks
