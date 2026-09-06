#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/ld2460/ld2460.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2460::testing {

// Mock UART component to satisfy UARTDevice parent requirement.
class MockUARTComponent : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {}
  MOCK_METHOD(bool, read_array, (uint8_t * data, size_t len), (override));
  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(size_t, available, (), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

// Expose protected members for testing.
class TestableLD2460 : public LD2460Component {
 public:
  using LD2460Component::buffer_data_;
  using LD2460Component::buffer_pos_;
  using LD2460Component::readline_;
  using LD2460Component::target_info_;
  using LD2460Component::installation_mode_;
  using LD2460Component::detection_distance_;

  void feed(const std::vector<uint8_t> &data) {
    for (uint8_t byte : data) {
      this->readline_(byte);
    }
  }
};

// LD2460 periodic data frame:
// Header (4) + Func (1) + Len (2) + Targets * 4 + Footer (4)
inline std::vector<uint8_t> make_periodic_frame(uint8_t num_targets = 1) {
  uint16_t len = 11 + num_targets * 4;
  std::vector<uint8_t> frame = {
      0xF4, 0xF3, 0xF2, 0xF1, 0x04, static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>((len >> 8) & 0xFF),
  };
  for (int i = 0; i < num_targets; i++) {
    // Target X = 15 (1.5m), Target Y = 23 (2.3m)
    frame.push_back(0x0F);
    frame.push_back(0x00);
    frame.push_back(0x17);
    frame.push_back(0x00);
  }
  frame.push_back(0xF8);
  frame.push_back(0xF7);
  frame.push_back(0xF6);
  frame.push_back(0xF5);
  return frame;
}

// LD2460 command ACK frame for CMD_SET_REPORTING (0x06), successful.
inline std::vector<uint8_t> make_ack_frame() {
  return {
      0xFD, 0xFC, 0xFB, 0xFA,  // CMD_FRAME_HEADER
      0x06,                    // command = reporting
      0x0C, 0x00,              // length = 12
      0x11,                    // result = ok, enabled
      0x04, 0x03, 0x02, 0x01   // CMD_FRAME_FOOTER
  };
}

}  // namespace esphome::ld2460::testing
