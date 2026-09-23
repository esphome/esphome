#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/uart/uart_component.h"
#include "esphome/components/ufm01/ufm01.h"

namespace esphome::ufm01::testing {

static constexpr uint8_t FRAME_START_BYTE_1 = 0x3C;
static constexpr uint8_t FRAME_START_BYTE_2 = 0x32;
static constexpr uint8_t PASSIVE_START_BYTE_2 = 0x64;
static constexpr uint8_t FRAME_STOP_BYTE = 0x16;
static constexpr uint8_t FRAME_FLAG_INSTANT_FLOW = 0x0B;
static constexpr uint8_t FRAME_FLAG_RESERVED_SECTION = 0x0C;
static constexpr uint8_t FRAME_FLAG_TEMP = 0x0D;
static constexpr uint8_t COMMAND_ACK = 0xE5;

// UART mock with a byte queue for read-side simulation.
class QueuedMockUART : public uart::UARTComponent {
 public:
  std::deque<uint8_t> rx_queue;
  std::vector<uint8_t> written_data;

  void enqueue(const std::vector<uint8_t> &data) {
    this->rx_queue.insert(this->rx_queue.end(), data.begin(), data.end());
  }

  void enqueue(std::initializer_list<uint8_t> data) {
    for (uint8_t byte : data)
      this->rx_queue.push_back(byte);
  }

  void clear_rx() { this->rx_queue.clear(); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_queue.size() < len)
      return false;
    for (size_t i = 0; i < len; ++i) {
      data[i] = this->rx_queue.front();
      this->rx_queue.pop_front();
    }
    return true;
  }

  bool peek_byte(uint8_t *data) override {
    if (this->rx_queue.empty())
      return false;
    *data = this->rx_queue.front();
    return true;
  }

  size_t available() override { return this->rx_queue.size(); }

  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  void write_array(const uint8_t *data, size_t len) override { this->written_data.assign(data, data + len); }

  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

class TestableUFM01 : public UFM01Component {
 public:
  void set_mock_uart(QueuedMockUART *uart) { this->set_uart_parent(uart); }

  bool process_active_stream() { return this->process_active_stream_(); }

  PassiveReadResult continue_passive_read() { return this->continue_passive_read_(); }

  bool consume_ack() { return this->consume_ack_(); }

  void start_passive_read() { this->start_passive_read_(); }

  void loop_startup() { this->loop_startup_(); }

  OperatingMode operating_mode() const { return this->operating_mode_; }

  StartupPhase startup_phase() const { return this->startup_phase_; }

  int32_t read_index() const { return this->read_index_; }

  size_t passive_index() const { return this->passive_index_; }

  uint32_t last_valid_frame_ms() const { return this->last_valid_frame_ms_; }

  void prepare_passive_read() {
    this->passive_index_ = 0;
    this->passive_start_ms_ = millis();
  }

  void init_wait_phase() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::WAIT;
    this->startup_wait_ms_ = 60000;
    this->phase_start_ms_ = millis();
  }

  void reset_state() {
    this->read_index_ = 0;
    this->last_valid_frame_ms_ = 0;
    this->passive_index_ = 0;
    this->passive_read_pending_ = false;
  }
};

inline std::array<uint8_t, FRAME_SIZE> make_active_frame() {
  std::array<uint8_t, FRAME_SIZE> frame{};
  frame[0] = FRAME_START_BYTE_1;
  frame[1] = FRAME_START_BYTE_2;
  frame[15] = FRAME_FLAG_INSTANT_FLOW;
  frame[21] = FRAME_FLAG_RESERVED_SECTION;
  frame[24] = FRAME_FLAG_TEMP;
  frame[31] = FRAME_STOP_BYTE;
  uint8_t sum = 0;
  for (size_t i = 0; i < 30; ++i)
    sum += frame[i];
  frame[30] = sum;
  return frame;
}

inline std::array<uint8_t, PASSIVE_FRAME_SIZE> make_passive_frame() {
  std::array<uint8_t, PASSIVE_FRAME_SIZE> frame{};
  frame[0] = FRAME_START_BYTE_1;
  frame[1] = PASSIVE_START_BYTE_2;
  frame[9] = FRAME_FLAG_INSTANT_FLOW;
  frame[15] = FRAME_FLAG_TEMP;
  frame[22] = FRAME_STOP_BYTE;
  uint8_t sum = 0;
  for (size_t i = 0; i < 21; ++i)
    sum += frame[i];
  frame[21] = sum;
  return frame;
}

class UFM01Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->mock_uart_.clear_rx();
    this->mock_uart_.written_data.clear();
    this->ufm01_.set_mock_uart(&this->mock_uart_);
    this->ufm01_.reset_state();
  }

  QueuedMockUART mock_uart_;
  TestableUFM01 ufm01_;
};

}  // namespace esphome::ufm01::testing
