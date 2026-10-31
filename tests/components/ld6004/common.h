#pragma once

#ifdef USE_HOST

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/ld6004/ld6004.h"
#include "esphome/components/ld600x/ld600x_frame.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::ld6004::testing {

using ld600x::AREA_COUNT;
static constexpr uint8_t MAX_TARGETS = LD600X_MAX_TARGETS;

static constexpr uint16_t TYPE_CONTROL = 0x0201;
static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;
static constexpr uint16_t TYPE_REPORT_AREA_PRESENCE = 0x0A0A;
static constexpr uint16_t TYPE_REPORT_WORK_MODE = 0x0A12;
static constexpr uint16_t TYPE_REPORT_WORK_STATUS = 0x0A14;

static constexpr uint32_t CMD_POINT_CLOUD_OFF = 0x07;
static constexpr uint32_t CMD_TARGET_DISPLAY_ON = 0x08;
// Every frame this component sends has the top bit of its id set.
static constexpr uint16_t FIRST_COMMAND_ID = 0x8000;

// The 0x0A12 values, in the order of the work_mode select options.
enum WorkMode : uint8_t {
  WORK_MODE_NORMAL,
  WORK_MODE_LOW_POWER,
  WORK_MODE_RADAR_OFF_P20_HIGH,
  WORK_MODE_RADAR_OFF_P20_LOW,
  WORK_MODE_HIGH_REFLECTIVITY,
  WORK_MODE_COUNT,
};

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
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
};

inline void put_u32(std::vector<uint8_t> &out, uint32_t value) {
  for (int i = 0; i < 4; i++)
    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

inline void put_f32(std::vector<uint8_t> &out, float value) { put_u32(out, bit_cast<uint32_t>(value)); }

inline std::vector<uint8_t> tf_frame(uint16_t type, const std::vector<uint8_t> &payload, uint16_t id = 0x0001) {
  std::vector<uint8_t> out(ld600x::TF_FRAME_OVERHEAD + payload.size());
  out.resize(ld600x::encode_frame(id, type, payload.empty() ? nullptr : payload.data(),
                                  static_cast<uint16_t>(payload.size()), out.data()));
  return out;
}

// An ACK is the command's own type with an empty payload.
inline std::vector<uint8_t> ack(uint16_t type) { return tf_frame(type, {}); }

inline std::vector<uint8_t> u32_payload(uint32_t value) {
  std::vector<uint8_t> p;
  put_u32(p, value);
  return p;
}

// What the component is expected to put on the wire for a command
inline std::vector<uint8_t> command(uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  return tf_frame(type, payload, id);
}

inline std::vector<uint8_t> work_mode(uint8_t mode) { return tf_frame(TYPE_REPORT_WORK_MODE, {mode}); }

// One target at (1, 1, 1) with the given cluster id, or an empty report when count is 0.
inline std::vector<uint8_t> target_report(uint8_t count) {
  std::vector<uint8_t> p;
  put_u32(p, count);
  for (uint8_t i = 0; i < count; i++) {
    put_f32(p, 1.0f);
    put_f32(p, 1.0f);
    put_f32(p, 1.0f);
    put_u32(p, 0);
    put_u32(p, 10 + i);
  }
  return tf_frame(TYPE_REPORT_TARGET, p);
}

inline std::vector<uint8_t> area_presence(const std::array<uint32_t, AREA_COUNT> &states) {
  std::vector<uint8_t> p;
  for (uint32_t s : states)
    put_u32(p, s);
  return tf_frame(TYPE_REPORT_AREA_PRESENCE, p);
}

/// A hub with the presence binary sensors, set up as codegen does and past its boot timeout.
class LD6004Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = std::filesystem::temp_directory_path() / "esphome_ld6004_test";
    std::filesystem::remove_all(this->temp_dir_);
    std::filesystem::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    host::host_preferences = &this->prefs_;
    global_preferences = &this->prefs_;

    this->sut_.set_uart_parent(&this->uart_);
    this->configure_();

    this->sut_.setup();
    // Past setup()'s 100 ms boot timeout, which starts the streams and queues the queries
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    App.scheduler.call(millis());
  }

  /// Attach the entities under test before setup(); the default is the presence hub.
  virtual void configure_() {
    this->sut_.set_presence_binary_sensor(&this->presence_);
    for (uint8_t i = 0; i < MAX_TARGETS; i++)
      this->sut_.set_target_presence_binary_sensor(i, &this->target_presence_[i]);
    for (uint8_t i = 0; i < AREA_COUNT; i++)
      this->sut_.set_area_presence_binary_sensor(i, &this->area_presence_[i]);
  }

  void TearDown() override {
    host::setup_preferences();
    unsetenv("ESPHOME_PREFDIR");
    std::filesystem::remove_all(this->temp_dir_);
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  host::HostPreferences prefs_;
  std::filesystem::path temp_dir_;
  MockUARTComponent uart_;
  LD6004Component sut_;

  binary_sensor::BinarySensor presence_;
  binary_sensor::BinarySensor target_presence_[MAX_TARGETS];
  binary_sensor::BinarySensor area_presence_[AREA_COUNT];
};

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
