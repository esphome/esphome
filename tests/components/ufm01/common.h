#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/uart/uart_component.h"
#include "esphome/components/ufm01/ufm01.h"
#include "esphome/core/application.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::ufm01::testing {

static constexpr uint8_t FRAME_START_BYTE_1 = 0x3C;
static constexpr uint8_t FRAME_START_BYTE_2 = 0x32;
static constexpr uint8_t PASSIVE_START_BYTE_2 = 0x64;
static constexpr uint8_t PASSIVE_START_BYTE_2_WITH_ID = 0x96;
static constexpr uint8_t FRAME_STOP_BYTE = 0x16;
static constexpr uint8_t FRAME_FLAG_INSTANT_FLOW = 0x0B;
static constexpr uint8_t FRAME_FLAG_RESERVED_SECTION = 0x0C;
static constexpr uint8_t FRAME_FLAG_TEMP = 0x0D;
static constexpr uint8_t COMMAND_ACK = 0xE5;
// Matches PASSIVE_FAIL_ESCALATE_COUNT in ufm01.cpp
static constexpr uint8_t ESCALATE_COUNT = 8;

using Command = std::array<uint8_t, 7>;
static constexpr Command ACTIVE_MODE = {0xFE, 0xFE, 0x11, 0x5C, 0x00, 0x5C, 0x16};
static constexpr Command PASSIVE_MODE = {0xFE, 0xFE, 0x11, 0x5C, 0x01, 0x5D, 0x16};
static constexpr Command CLEAR_ACCUMULATED_FLOW = {0xFE, 0xFE, 0x11, 0x5A, 0xFD, 0x57, 0x16};
static constexpr Command RESET_DEVICE = {0xFE, 0xFE, 0x11, 0x5D, 0xCB, 0x28, 0x16};
static constexpr Command READ_SENSOR_DATA_NO_ID = {0xFE, 0xFE, 0x11, 0x5B, 0x0F, 0x6A, 0x16};
static constexpr Command READ_SENSOR_DATA_WITH_ID = {0xFE, 0xFE, 0x11, 0x5B, 0xCB, 0x26, 0x16};
static constexpr Command GET_SOFTWARE_VERSION = {0xFE, 0xFE, 0x11, 0x5E, 0x62, 0xC0, 0x16};

// UART mock with a byte queue for read-side simulation.
class QueuedMockUART : public uart::UARTComponent {
 public:
  std::deque<uint8_t> rx_queue;
  std::vector<uint8_t> written_data;

  void enqueue(const std::vector<uint8_t> &data) {
    this->rx_queue.insert(this->rx_queue.end(), data.begin(), data.end());
  }

  template<size_t N> void enqueue(const std::array<uint8_t, N> &data) {
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

  static uint32_t now() { return App.get_loop_component_start_time(); }

  bool process_active_stream() { return this->process_active_stream_(now()); }

  ReadResult continue_passive_read() { return this->continue_passive_read_(now()); }

  bool consume_ack() { return this->consume_ack_(); }

  void start_passive_read() { this->start_passive_read_(now()); }

  void loop_startup() { this->loop_startup_(now()); }

  void loop_active_stream() { this->loop_active_stream_(now()); }

  void loop_passive_poll() { this->loop_passive_poll_(now()); }

  void loop_pending_clear_action() { this->loop_pending_clear_action_(now()); }

  bool clear_pending() const { return this->clear_pending_; }

  bool pending_clear_sent() const { return this->pending_clear_sent_; }

  bool meter_id_published() const { return this->meter_id_published_; }

  void start_software_version_read() { this->start_software_version_read_(now()); }

  ReadResult continue_software_version_read() { return this->continue_software_version_read_(now()); }

  bool software_version_published() const { return this->software_version_published_; }

  void set_last_software_version_attempt_ms(uint32_t ms) { this->last_software_version_attempt_ms_ = ms; }

  OperatingMode operating_mode() const { return this->operating_mode_; }

  StartupPhase startup_phase() const { return this->startup_phase_; }

  int32_t read_index() const { return this->read_index_; }

  uint8_t passive_index() const { return this->passive_index_; }

  uint32_t last_valid_frame_ms() const { return this->last_valid_frame_ms_; }

  uint8_t consecutive_passive_failures() const { return this->consecutive_passive_failures_; }

  void set_operating_mode(OperatingMode mode) { this->operating_mode_ = mode; }

  // Expects a reply without the meter ID, as start_passive_read_() picks when no meter ID sensor is set
  void prepare_passive_read() {
    this->passive_index_ = 0;
    this->passive_expected_size_ = PASSIVE_FRAME_SIZE;
    this->passive_start_byte_2_ = PASSIVE_START_BYTE_2;
    this->passive_start_ms_ = now();
  }

  void begin_pending_passive_read() {
    this->prepare_passive_read();
    this->passive_read_pending_ = true;
  }

  void init_wait_phase() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::WAIT;
    this->startup_wait_ms_ = 60000;
    this->phase_start_ms_ = now();
  }

  void begin_active_stream() {
    this->operating_mode_ = OperatingMode::ACTIVE_STREAM;
    this->last_valid_frame_ms_ = now();
    this->passive_read_pending_ = false;
    this->read_index_ = 0;
  }

  void begin_active_wait_frame() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::ACTIVE_WAIT_FRAME;
    this->phase_start_ms_ = now();
  }

  void begin_post_reset_wait() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::POST_RESET_WAIT;
    this->phase_start_ms_ = now();
    this->software_version_published_ = false;
  }

  void reset_state() {
    this->read_index_ = 0;
    this->last_valid_frame_ms_ = 0;
    this->passive_index_ = 0;
    this->passive_read_pending_ = false;
    this->operating_mode_ = OperatingMode::STARTUP;
    this->consecutive_passive_failures_ = 0;
    this->meter_id_published_ = false;
    this->software_version_published_ = false;
    this->software_version_read_pending_ = false;
    this->software_version_failures_ = 0;
    this->meter_id_invalid_logged_ = false;
    this->last_software_version_attempt_ms_ = 0;
    this->startup_phase_ = StartupPhase::WAIT;
    this->clear_pending_ = false;
    this->pending_clear_sent_ = false;
  }
};

template<size_t N> inline uint8_t checksum(const std::array<uint8_t, N> &data, size_t begin, size_t end) {
  uint8_t sum = 0;
  for (size_t i = begin; i < end; ++i)
    sum += data[i];
  return sum;
}

inline std::array<uint8_t, FRAME_SIZE> make_active_frame() {
  std::array<uint8_t, FRAME_SIZE> frame{};
  frame[0] = FRAME_START_BYTE_1;
  frame[1] = FRAME_START_BYTE_2;
  // Datasheet example device ID 2307140001
  frame[2] = 0x01;
  frame[3] = 0x00;
  frame[4] = 0x14;
  frame[5] = 0x07;
  frame[6] = 0x23;
  frame[7] = 0x01;
  frame[15] = FRAME_FLAG_INSTANT_FLOW;
  frame[21] = FRAME_FLAG_RESERVED_SECTION;
  frame[24] = FRAME_FLAG_TEMP;
  frame[31] = FRAME_STOP_BYTE;
  frame[30] = checksum(frame, 0, 30);
  return frame;
}

inline std::array<uint8_t, SOFTWARE_VERSION_RESPONSE_SIZE> make_software_version_response() {
  // Datasheet example version 23380315
  std::array<uint8_t, SOFTWARE_VERSION_RESPONSE_SIZE> response{};
  response[0] = COMMAND_ACK;
  response[1] = 0x15;
  response[2] = 0x03;
  response[3] = 0x38;
  response[4] = 0x23;
  response[5] = checksum(response, 1, 5);
  response[6] = FRAME_STOP_BYTE;
  return response;
}

inline std::array<uint8_t, PASSIVE_FRAME_SIZE> make_passive_frame() {
  std::array<uint8_t, PASSIVE_FRAME_SIZE> frame{};
  frame[0] = FRAME_START_BYTE_1;
  frame[1] = PASSIVE_START_BYTE_2;
  frame[9] = FRAME_FLAG_INSTANT_FLOW;
  frame[15] = FRAME_FLAG_TEMP;
  frame[22] = FRAME_STOP_BYTE;
  frame[21] = checksum(frame, 0, 21);
  return frame;
}

inline std::array<uint8_t, PASSIVE_FRAME_WITH_ID_SIZE> make_passive_with_id_frame() {
  std::array<uint8_t, PASSIVE_FRAME_WITH_ID_SIZE> frame{};
  frame[0] = FRAME_START_BYTE_1;
  frame[1] = PASSIVE_START_BYTE_2_WITH_ID;
  frame[2] = 0x01;
  frame[3] = 0x00;
  frame[4] = 0x14;
  frame[5] = 0x07;
  frame[6] = 0x23;
  frame[7] = 0x01;
  frame[8] = 0x0A;
  // Non-measurement bytes ensure a shifted instant-flow offset is detected.
  for (size_t i = 17; i < 22; ++i)
    frame[i] = 0x99;
  frame[22] = FRAME_FLAG_INSTANT_FLOW;
  frame[23] = 0x34;
  frame[24] = 0x12;
  frame[27] = 0x00;
  frame[31] = FRAME_FLAG_TEMP;
  frame[32] = 0x50;
  frame[33] = 0x23;
  frame[38] = FRAME_STOP_BYTE;
  frame[37] = checksum(frame, 0, 37);
  return frame;
}

class UFM01Test : public ::testing::Test {
 protected:
  // Non-zero so a frame received at the start time still counts as a valid timestamp
  static constexpr uint32_t START_MS = 1000;

  void SetUp() override {
    this->mock_uart_.clear_rx();
    this->mock_uart_.written_data.clear();
    this->ufm01_.set_mock_uart(&this->mock_uart_);
    this->ufm01_.reset_state();
    this->set_clock(START_MS);
  }

  // Publishes the time the component reads as the loop clock
  void set_clock(uint32_t ms) {
    this->now_ = ms;
    LoopBlockingGuard clock{nullptr, nullptr, ms};
  }

  void advance(uint32_t ms) { this->set_clock(this->now_ + ms); }

  void expect_written(const Command &command) {
    EXPECT_EQ(this->mock_uart_.written_data, std::vector<uint8_t>(command.begin(), command.end()));
  }

  void fail_passive_poll() {
    this->ufm01_.begin_pending_passive_read();
    this->advance(2000);
    this->ufm01_.loop_passive_poll();
  }

  void make_active_stream_stale() {
    this->ufm01_.begin_active_stream();
    this->advance(6000);
  }

#ifdef USE_SENSOR
  void attach_flow_and_temperature_sensors() {
    this->ufm01_.set_flow_sensor(&this->flow_sensor_);
    this->ufm01_.set_temperature_sensor(&this->temperature_sensor_);
    this->flow_sensor_.publish_state(1.23f);
    this->temperature_sensor_.publish_state(20.5f);
  }

  sensor::Sensor flow_sensor_;
  sensor::Sensor temperature_sensor_;
#endif

  void attach_diagnostic_text_sensors() {
    this->ufm01_.set_meter_id_text_sensor(&this->meter_id_text_sensor_);
    this->ufm01_.set_software_version_text_sensor(&this->software_version_text_sensor_);
  }

  text_sensor::TextSensor meter_id_text_sensor_;
  text_sensor::TextSensor software_version_text_sensor_;

  uint32_t now_{0};
  QueuedMockUART mock_uart_;
  TestableUFM01 ufm01_;
};

}  // namespace esphome::ufm01::testing
