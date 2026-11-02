#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "esphome/components/uart/uart_component.h"

namespace esphome::jsn_sr04t::testing {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void write_array(const uint8_t *data, size_t len) override {}
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

}  // namespace esphome::jsn_sr04t::testing
