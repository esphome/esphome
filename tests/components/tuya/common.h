#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include "esphome/components/uart/uart_component.h"

namespace esphome::tuya::testing {

// UART with an in-memory receive queue and a record of the bytes written
class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }

  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

}  // namespace esphome::tuya::testing
