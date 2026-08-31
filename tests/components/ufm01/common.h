#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/uart/uart_component.h"
#include "esphome/components/ufm01/ufm01.h"
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

  void loop_active_stream() { this->loop_active_stream_(); }

  void loop_entering_passive() { this->loop_entering_passive_(); }

  void loop_passive_poll() { this->loop_passive_poll_(); }

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  void loop_pending_clear_action() { this->loop_pending_clear_action_(); }

  void request_clear_accumulated_flow(ClearAccumulatedFlowActionInterface *action) {
    this->request_clear_accumulated_flow_(action);
  }

  void cancel_pending_clear_action(ClearAccumulatedFlowActionInterface *action) {
    this->cancel_pending_clear_action_(action);
  }

  bool pending_clear_sent() const { return this->pending_clear_sent_; }

  void set_pending_clear_start_ms(uint32_t ms) { this->pending_clear_start_ms_ = ms; }
#endif

#ifdef USE_UFM01_DEVICE_ID
  bool device_id_published() const { return this->device_id_published_; }
#endif

#ifdef USE_TEXT_SENSOR
  void start_software_version_read() { this->start_software_version_read_(); }

  SoftwareVersionReadResult continue_software_version_read() { return this->continue_software_version_read_(); }

  bool software_version_published() const { return this->software_version_published_; }

  void set_software_version_start_ms(uint32_t ms) { this->software_version_start_ms_ = ms; }
#endif

  OperatingMode operating_mode() const { return this->operating_mode_; }

  StartupPhase startup_phase() const { return this->startup_phase_; }

  int32_t read_index() const { return this->read_index_; }

  size_t passive_index() const { return this->passive_index_; }

  uint32_t last_valid_frame_ms() const { return this->last_valid_frame_ms_; }

  uint8_t consecutive_passive_failures() const { return this->consecutive_passive_failures_; }

  void set_operating_mode(OperatingMode mode) { this->operating_mode_ = mode; }

  void prepare_passive_read() {
    this->passive_index_ = 0;
    this->passive_start_ms_ = millis();
  }

  void begin_pending_passive_read() {
    this->prepare_passive_read();
    this->passive_read_pending_ = true;
  }

  void prepare_timed_out_passive_read() {
    this->passive_index_ = 0;
    this->passive_start_ms_ = millis() - 2000;
    this->passive_read_pending_ = true;
  }

  void init_wait_phase() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::WAIT;
    this->startup_wait_ms_ = 60000;
    this->phase_start_ms_ = millis();
  }

  void prepare_stale_active_stream() {
    this->operating_mode_ = OperatingMode::ACTIVE_STREAM;
    // Far enough in the past that ACTIVE_STALE_MS has elapsed
    this->last_valid_frame_ms_ = millis() - 6000;
    this->passive_read_pending_ = false;
    this->read_index_ = 0;
  }

  void prepare_active_wait_timeout() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::ACTIVE_WAIT_FRAME;
    this->phase_start_ms_ = millis() - 4000;
  }

  void prepare_post_reset_wait_phase() {
    this->operating_mode_ = OperatingMode::STARTUP;
    this->startup_phase_ = StartupPhase::POST_RESET_WAIT;
    this->phase_start_ms_ = millis() - 3000;
#ifdef USE_TEXT_SENSOR
    this->software_version_published_ = false;
#endif
  }

  void reset_state() {
    this->read_index_ = 0;
    this->last_valid_frame_ms_ = 0;
    this->passive_index_ = 0;
    this->passive_read_pending_ = false;
    this->operating_mode_ = OperatingMode::STARTUP;
    this->consecutive_passive_failures_ = 0;
#ifdef USE_UFM01_DEVICE_ID
    this->device_id_published_ = false;
#endif
#ifdef USE_TEXT_SENSOR
    this->software_version_published_ = false;
    this->software_version_index_ = 0;
    this->software_version_read_pending_ = false;
#endif
    this->startup_phase_ = StartupPhase::WAIT;
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
    this->pending_clear_action_ = nullptr;
    this->pending_clear_sent_ = false;
#endif
  }
};

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
  uint8_t sum = 0;
  for (size_t i = 0; i < 30; ++i)
    sum += frame[i];
  frame[30] = sum;
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
  uint8_t sum = 0;
  for (size_t i = 1; i < 5; ++i)
    sum += response[i];
  response[5] = sum;
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
  uint8_t sum = 0;
  for (size_t i = 0; i < 21; ++i)
    sum += frame[i];
  frame[21] = sum;
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
  uint8_t sum = 0;
  for (size_t i = 0; i < 37; ++i)
    sum += frame[i];
  frame[37] = sum;
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

#ifdef USE_TEXT_SENSOR
  void attach_diagnostic_text_sensors() {
    this->ufm01_.set_device_id_text_sensor(&this->device_id_text_sensor_);
    this->ufm01_.set_software_version_text_sensor(&this->software_version_text_sensor_);
  }

  text_sensor::TextSensor device_id_text_sensor_;
  text_sensor::TextSensor software_version_text_sensor_;
#endif

  QueuedMockUART mock_uart_;
  TestableUFM01 ufm01_;
};

}  // namespace esphome::ufm01::testing
