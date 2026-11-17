#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "esphome/components/uart/uart_component.h"

namespace esphome::pioneer_wt::testing {

inline std::vector<uint8_t> from_hex(const std::string &hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return out;
}

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;

  void push_rx(const std::vector<uint8_t> &data) { this->rx.insert(this->rx.end(), data.begin(), data.end()); }
  void push_rx_hex(const std::string &hex) { this->push_rx(from_hex(hex)); }

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

}  // namespace esphome::pioneer_wt::testing
