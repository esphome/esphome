#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105::testing {

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
};

class TestableMitsubishiCN105 : public MitsubishiCN105 {
 public:
  using MitsubishiCN105::MitsubishiCN105;
  using MitsubishiCN105::State;
  using MitsubishiCN105::UpdateFlag;
  using MitsubishiCN105::state_;
  using MitsubishiCN105::operation_start_ms_;
  using MitsubishiCN105::use_temperature_encoding_b_;
  using MitsubishiCN105::status_update_wait_credit_ms_;
  using MitsubishiCN105::pending_updates_;

  void set_state(State s) { this->set_state_(s); }
  void apply_settings() { this->apply_settings_(); }

  static inline uint32_t test_loop_time_ms = 0;

  void set_current_time(uint32_t ms) { test_loop_time_ms = ms; }
};

}  // namespace esphome::mitsubishi_cn105::testing
