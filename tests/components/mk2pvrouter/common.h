#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/mk2pvrouter/mk2pvrouter.h"

namespace esphome::mk2pvrouter::testing {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;

  void push_rx(std::initializer_list<uint8_t> data) { this->rx.insert(this->rx.end(), data.begin(), data.end()); }

  // UARTComponent
  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }

    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)
};

class TestableMk2PVRouter : public Mk2PVRouter {
 public:
  using Mk2PVRouter::Mk2PVRouter;
  using Mk2PVRouter::State;
  using Mk2PVRouter::buf_;
  using Mk2PVRouter::buf_index_;
  using Mk2PVRouter::calculate_crc_;
  using Mk2PVRouter::check_crc_;
  using Mk2PVRouter::process_group_;
  using Mk2PVRouter::state_;
  using Mk2PVRouter::tag_;
  using Mk2PVRouter::val_;
};

}  // namespace esphome::mk2pvrouter::testing
