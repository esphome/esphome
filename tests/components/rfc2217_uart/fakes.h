#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "esphome/components/rfc2217_uart/rfc2217_uart.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::rfc2217_uart::testing {

/// The tcp_uart side: what the peer sent, and what this side wrote.
class Pipe : public tcp_uart::TcpUart {
 public:
  bool is_connected() override { return this->up_; }
  size_t available() override { return this->rx_n_; }
  size_t available_for_write() override { return this->room_; }
  bool peek_byte(uint8_t *data) override {
    if (this->rx_n_ == 0) {
      return false;
    }
    *data = this->rx_[0];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_n_) {
      return false;
    }
    std::memcpy(data, this->rx_, len);
    this->rx_n_ -= len;
    std::memmove(this->rx_, this->rx_ + len, this->rx_n_);
    return true;
  }
  void write_array(const uint8_t *data, size_t len) override {
    ASSERT_LE(this->n_ + len, sizeof(this->buf_));
    std::memcpy(this->buf_ + this->n_, data, len);
    this->n_ += len;
  }
  uart::UARTFlushResult flush() override {
    this->flushes_++;
    return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  }

  void feed(const uint8_t *data, size_t len) {
    ASSERT_LE(this->rx_n_ + len, sizeof(this->rx_));
    std::memcpy(this->rx_ + this->rx_n_, data, len);
    this->rx_n_ += len;
  }
  template<size_t N> void feed(const uint8_t (&data)[N]) { this->feed(data, N); }
  void clear() { this->n_ = 0; }
  /// Whether the bytes this side wrote contain needle.
  template<size_t N> bool sent(const uint8_t (&needle)[N]) const {
    for (size_t i = 0; i + N <= this->n_; i++) {
      if (std::memcmp(this->buf_ + i, needle, N) == 0) {
        return true;
      }
    }
    return false;
  }

  bool up_{true};
  size_t room_{1024};
  size_t n_{0};
  int flushes_{0};
  uint8_t buf_[1024]{};
  size_t rx_n_{0};
  uint8_t rx_[512]{};
};

/// The hardware UART of the server.
class FakeSerial : public uart::UARTComponent {
 public:
  FakeSerial() {
    this->baud_rate_ = 9600;
    this->data_bits_ = 8;
    this->stop_bits_ = 1;
    this->parity_ = uart::UART_CONFIG_PARITY_NONE;
  }
  void write_array(const uint8_t *data, size_t len) override {
    ASSERT_LE(this->n_ + len, sizeof(this->buf_));
    std::memcpy(this->buf_ + this->n_, data, len);
    this->n_ += len;
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx_n_ == 0) {
      return false;
    }
    *data = this->rx_[0];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_n_) {
      return false;
    }
    std::memcpy(data, this->rx_, len);
    this->rx_n_ -= len;
    std::memmove(this->rx_, this->rx_ + len, this->rx_n_);
    return true;
  }
  size_t available() override { return this->rx_n_; }
  size_t available_for_write() override { return this->room_; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

  void feed(const uint8_t *data, size_t len) {
    ASSERT_LE(this->rx_n_ + len, sizeof(this->rx_));
    std::memcpy(this->rx_ + this->rx_n_, data, len);
    this->rx_n_ += len;
  }

  size_t room_{1024};
  size_t n_{0};
  uint8_t buf_[1024]{};
  size_t rx_n_{0};
  uint8_t rx_[1024]{};

 protected:
  void check_logger_conflict() override {}
};

}  // namespace esphome::rfc2217_uart::testing
