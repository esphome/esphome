#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/uart/uart_component.h"

namespace esphome::smt100::testing {

// Records what the driver writes and serves queued bytes to its reads.
class FakeUART : public uart::UARTComponent {
 public:
  std::string tx;
  std::vector<uint8_t> rx;

  void receive(const std::string &text) { this->rx.insert(this->rx.end(), text.begin(), text.end()); }

  void write_array(const uint8_t *data, size_t len) override { this->tx.append(data, data + len); }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

}  // namespace esphome::smt100::testing
