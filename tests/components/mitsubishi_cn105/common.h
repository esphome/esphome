#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105.h"
#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_component.h"
#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_climate.h"

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

  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)
};

// Appends a frame and its checksum to the RX buffer.
template<std::size_t N> inline void push_frame(MockUARTComponent &uart, const std::array<uint8_t, N> &frame) {
  uart.rx.insert(uart.rx.end(), frame.begin(), frame.end());
  uint8_t sum = 0;
  for (const uint8_t value : frame) {
    sum += value;
  }
  uart.rx.push_back(static_cast<uint8_t>(0xFC - sum));
}

// 0x02 settings: target 24°C (encoding A), fan auto, vane position 4, wide vane swing. Mode 0x08 is AUTO.
inline void push_settings_response(MockUARTComponent &uart, bool power_on = false, uint8_t raw_mode = 0x08) {
  push_frame(uart, std::array<uint8_t, 21>{0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, power_on, raw_mode, 0x07,
                                           0x00, 0x04, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00,     0x00});
}

// 0x03 room temperature: 21°C.
inline void push_room_temperature_response(MockUARTComponent &uart) {
  push_frame(uart, std::array<uint8_t, 21>{0xFC, 0x62, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00,
                                           0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

// 0x06 operating status.
inline void push_operating_status_response(MockUARTComponent &uart, bool operating) {
  push_frame(uart, std::array<uint8_t, 21>{0xFC, 0x62, 0x01, 0x30, 0x10, 0x06, 0x00, 0x00, 0x00, operating, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

class TestableMitsubishiCN105 : public MitsubishiCN105 {
 public:
  using MitsubishiCN105::MitsubishiCN105;
  using MitsubishiCN105::State;
  using MitsubishiCN105::PropertyId;
  using MitsubishiCN105::state_;
  using MitsubishiCN105::status_;
  using MitsubishiCN105::operation_start_ms_;
  using MitsubishiCN105::property_context_;
  using MitsubishiCN105::status_update_wait_credit_ms_;
  using MitsubishiCN105::pending_updates_;
  using MitsubishiCN105::is_operating_status_unsupported_;

  void set_state(State s) { this->set_state_(s); }
  void apply_settings() { this->apply_settings_(); }

  static inline uint32_t test_loop_time_ms = 0;

  void set_current_time(uint32_t ms) { test_loop_time_ms = ms; }
};

}  // namespace esphome::mitsubishi_cn105::testing
