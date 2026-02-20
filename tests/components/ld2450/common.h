#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/ld2450/ld2450.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2450::testing {

// Mock UART component to satisfy UARTDevice parent requirement.
class MockUARTComponent : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {}
  MOCK_METHOD(bool, read_array, (uint8_t * data, size_t len), (override));
  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(size_t, available, (), (override));
  MOCK_METHOD(void, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
};

// Expose protected members for testing.
class TestableLD2450 : public LD2450Component {
 public:
  using LD2450Component::buffer_data_;
  using LD2450Component::buffer_pos_;
  using LD2450Component::readline_;

  void feed(const std::vector<uint8_t> &data) {
    for (uint8_t byte : data) {
      this->readline_(byte);
    }
  }
};

// LD2450 periodic data frame: header (4) + 3 targets * 8 bytes + footer (2) = 30 bytes
// All-zero targets means no presence detected.
inline std::vector<uint8_t> make_periodic_frame(uint8_t fill = 0x00) {
  std::vector<uint8_t> frame = {0xAA, 0xFF, 0x03, 0x00};  // DATA_FRAME_HEADER
  for (int i = 0; i < 24; i++) {
    frame.push_back(fill);  // 3 targets * 8 bytes
  }
  frame.push_back(0x55);  // DATA_FRAME_FOOTER
  frame.push_back(0xCC);
  return frame;
}

// LD2450 command ACK frame for CMD_ENABLE_CONF (0xFF), successful.
// header (4) + length (2) + command (2) + result (2) + footer (4) = 14 bytes
inline std::vector<uint8_t> make_ack_frame() {
  return {
      0xFD, 0xFC, 0xFB, 0xFA,  // CMD_FRAME_HEADER
      0x04, 0x00,              // length = 4
      0xFF, 0x01,              // command = enable_conf, status = success
      0x00, 0x00,              // result = ok
      0x04, 0x03, 0x02, 0x01   // CMD_FRAME_FOOTER
  };
}

}  // namespace esphome::ld2450::testing
