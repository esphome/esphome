#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/apc_proteous/apc_proteous_cover.h"

namespace esphome::apc_proteous::testing {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;

  void push_rx(const char *data) { this->rx.insert(this->rx.end(), data, data + strlen(data)); }
  std::string tx_string() const { return std::string(this->tx.begin(), this->tx.end()); }

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

class TestableAPCProteousCover : public APCProteousCover {
 public:
  using APCProteousCover::COMMAND_ACK_MS;
  using APCProteousCover::COMMAND_QUIET_MS;
  using APCProteousCover::MAX_COMMAND_RETRIES;
  using APCProteousCover::MAX_RESPONSE_LEN;
  using APCProteousCover::command_retries_;
  using APCProteousCover::last_command_tx_;
  using APCProteousCover::pending_command_;
  using APCProteousCover::pending_command_time_;
  using APCProteousCover::rx_len_;
  using APCProteousCover::target_position_;
};

}  // namespace esphome::apc_proteous::testing
