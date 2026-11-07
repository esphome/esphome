#pragma once
#include <cstdint>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/ld2460/ld2460.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2460::testing {

// Mock UART component to satisfy UARTDevice parent requirement.
class MockUARTComponent : public uart::UARTComponent {
 public:
  MOCK_METHOD(void, write_array, (const uint8_t *data, size_t len), (override));
  MOCK_METHOD(bool, read_array, (uint8_t *data, size_t len), (override));
  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(size_t, available, (), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

class TestableLD2460 : public LD2460Component {
 public:
  void feed(const std::vector<uint8_t> &data) {
    for (uint8_t byte : data) {
      this->readline_(byte);
    }
  }
};

struct Point {
  int16_t x;  // 0.1 m steps
  int16_t y;
};

// Periodic data frame: header (4) + function (1) + length (2) + 4 bytes per target + footer (4)
inline std::vector<uint8_t> make_periodic_frame(const std::vector<Point> &targets) {
  auto len = static_cast<uint16_t>(11 + targets.size() * 4);
  std::vector<uint8_t> frame = {
      0xF4, 0xF3, 0xF2, 0xF1, 0x04, static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>(len >> 8)};
  for (const auto &t : targets) {
    auto x = static_cast<uint16_t>(t.x);
    auto y = static_cast<uint16_t>(t.y);
    frame.insert(frame.end(), {static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>(x >> 8),
                               static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>(y >> 8)});
  }
  frame.insert(frame.end(), {0xF8, 0xF7, 0xF6, 0xF5});
  return frame;
}

// One target at (1.5 m, 2.3 m)
inline std::vector<uint8_t> make_periodic_frame() { return make_periodic_frame({{15, 23}}); }

// ACK for the query installation mode command (0x0A); mode 1 = side, 2 = top
inline std::vector<uint8_t> make_ack_frame(uint8_t mode) {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x0A, 0x0C, 0x00, mode, 0x04, 0x03, 0x02, 0x01};
}

}  // namespace esphome::ld2460::testing
