#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/ld2451/ld2451.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2451::testing {

// Mock UART component; bytes in rx are what the hub reads.
class MockUARTComponent : public uart::UARTComponent {
 public:
  MOCK_METHOD(void, write_array, (const uint8_t *data, size_t len), (override));
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

  std::vector<uint8_t> rx;
};

struct Target {
  int8_t angle;      // degrees
  uint8_t distance;  // m
  bool approaching;
  uint8_t speed;  // km/h
  uint8_t snr;
};

// Data frame: header (4), payload length (2, LE), target count, alarm, 5 bytes per target, footer (4)
inline std::vector<uint8_t> make_data_frame(const std::vector<Target> &targets, bool alarm = false) {
  auto len = static_cast<uint16_t>(2 + targets.size() * 5);
  std::vector<uint8_t> frame = {0xF4,
                                0xF3,
                                0xF2,
                                0xF1,
                                static_cast<uint8_t>(len & 0xFF),
                                static_cast<uint8_t>(len >> 8),
                                static_cast<uint8_t>(targets.size()),
                                static_cast<uint8_t>(alarm ? 0x01 : 0x00)};
  for (const auto &t : targets) {
    frame.insert(frame.end(), {static_cast<uint8_t>(t.angle + 0x80), t.distance,
                               static_cast<uint8_t>(t.approaching ? 0x01 : 0x00), t.speed, t.snr});
  }
  frame.insert(frame.end(), {0xF8, 0xF7, 0xF6, 0xF5});
  return frame;
}

// Data frame with no payload, as the radar may send while idle
inline std::vector<uint8_t> make_empty_data_frame() {
  return {0xF4, 0xF3, 0xF2, 0xF1, 0x00, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
}

// Successful ACK for the enable configuration command (0xFF)
inline std::vector<uint8_t> make_ack_frame() {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x08, 0x00, 0xFF, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
}

}  // namespace esphome::ld2451::testing
