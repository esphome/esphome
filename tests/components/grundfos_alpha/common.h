#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "esphome/components/genibus/genibus.h"
#include "esphome/components/grundfos_alpha/grundfos_alpha.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"

// The frames are built to the layout of a real ALPHA3: the requests and the reply headers (status, type, version,
// size) are the ones the alpha3 component reads from a real ALPHA3; the reply values are made up.
namespace esphome::grundfos_alpha::testing {

using Bytes = std::vector<uint8_t>;

inline const Bytes FRAME_IDENTITY = {0x27, 0x07, 0xE7, 0xF8, 0x02, 0x03, 0x94, 0x95, 0x96, 0xEB, 0x47};
inline const Bytes FRAME_PROCESS = {0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x52, 0x1F};
inline const Bytes FRAME_MOTOR = {0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45, 0x8A, 0xCD};
// Unit family/type/version 52/1/0 (ALPHA3 Model B) and 52/7/0 (ALPHA 15)
inline const Bytes REPLY_IDENTITY_ALPHA3 = {0x24, 0x07, 0xF8, 0xE7, 0x02, 0x03, 0x34, 0x01, 0x00, 0x03, 0x9E};
inline const Bytes REPLY_IDENTITY_ALPHA15 = {0x24, 0x07, 0xF8, 0xE7, 0x02, 0x03, 0x34, 0x07, 0x00, 0xA9, 0x38};
// Class 2 ACK 0 with only two data bytes (52/1, no version)
inline const Bytes REPLY_IDENTITY_SHORT = {0x24, 0x06, 0xF8, 0xE7, 0x02, 0x02, 0x34, 0x01, 0x35, 0x4A};
// Class 2 ACK 2: ID 148 unknown
inline const Bytes REPLY_IDENTITY_UNKNOWN_ID = {0x24, 0x05, 0xF8, 0xE7, 0x02, 0x81, 0x94, 0xDF, 0xA6};
// 93/289 type 0x0130 v1, 24 bytes: flow 2^-12 m³/s, head 39216 Pa, NaN, NaN, medium 25.5 °C, NaN
inline const Bytes REPLY_PROCESS = {0x24, 0x23, 0xF8, 0xE7, 0x0A, 0x1F, 0x00, 0x01, 0x30, 0x01, 0x00, 0x00, 0x18,
                                    0x39, 0x80, 0x00, 0x00, 0x47, 0x19, 0x30, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F,
                                    0xFF, 0xFF, 0xFF, 0x41, 0xCC, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xDD, 0x18};
// Class 10 ACK 2: object unknown, as the specification has it
inline const Bytes REPLY_PROCESS_ID_UNKNOWN = {0x24, 0x04, 0xF8, 0xE7, 0x0A, 0x80, 0x26, 0x99};
// Class 10 ACK 0 with the single data byte 4: object missing, as a real ALPHA3 answers
inline const Bytes REPLY_OBJECT_MISSING = {0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x01, 0x04, 0xEE, 0x26};
// 87/69 type 0x0100 v1, 37 bytes: 230 V, 325 V, 0.125 A, 20 W, 19.5 W, 3000 rpm, NaN, 40, 35, 1
inline const Bytes REPLY_MOTOR = {0x24, 0x30, 0xF8, 0xE7, 0x0A, 0x2C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x25,
                                  0x43, 0x66, 0x00, 0x00, 0x43, 0xA2, 0x80, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x41,
                                  0xA0, 0x00, 0x00, 0x41, 0x9C, 0x00, 0x00, 0x45, 0x3B, 0x80, 0x00, 0x7F, 0xFF,
                                  0xFF, 0xFF, 0x42, 0x20, 0x00, 0x00, 0x42, 0x0C, 0x00, 0x00, 0x01, 0x36, 0x93};
// The same values as type 0x0100 v2, 41 bytes, as other ALPHA models send it
inline const Bytes REPLY_MOTOR_V2 = {
    0x24, 0x34, 0xF8, 0xE7, 0x0A, 0x30, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x29, 0x43, 0x66, 0x00, 0x00, 0x43, 0xA2,
    0x80, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x41, 0xA0, 0x00, 0x00, 0x41, 0x9C, 0x00, 0x00, 0x45, 0x3B, 0x80, 0x00, 0x7F,
    0xFF, 0xFF, 0xFF, 0x42, 0x20, 0x00, 0x00, 0x42, 0x0C, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x56, 0xDD};

inline constexpr uint32_t TIMEOUT_MS = 1000;

class TestBus : public genibus::GenibusBus {
 public:
  bool is_connected() const override { return this->connected; }
  void lose_connection() {
    this->connected = false;
    this->connection_lost_();
  }
  void establish_connection() {
    this->connected = true;
    this->connection_established_();
  }

  bool connected{true};
  std::vector<Bytes> written;

 protected:
  bool write_frame(std::span<const uint8_t> frame) override {
    this->written.emplace_back(frame.begin(), frame.end());
    return true;
  }
};

class GrundfosAlphaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_master_address(0xF8);
    this->pump_.set_parent(&this->bus_);
    this->pump_.set_address(0xE7);
    this->bus_.register_device(&this->pump_);
    this->pump_.set_flow_sensor(&this->flow_);
    this->pump_.set_head_sensor(&this->head_);
    this->pump_.set_medium_temperature_sensor(&this->medium_temperature_);
    this->pump_.set_voltage_sensor(&this->voltage_);
    this->pump_.set_current_sensor(&this->current_);
    this->pump_.set_power_sensor(&this->power_);
    this->pump_.set_speed_sensor(&this->speed_);
  }

  void run_(uint32_t ms = 16) {
    this->now_ += ms;
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    this->bus_.loop();
  }

  // Run the bus until it writes, and return what it wrote; empty when it writes nothing
  Bytes next_request_() {
    const size_t before = this->bus_.written.size();
    this->run_();
    if (this->bus_.written.size() == before)
      return {};
    return this->bus_.written.back();
  }

  void reply_(const Bytes &frame) { this->bus_.receive_bytes(frame); }

  // Identify the pump as an ALPHA3 and answer both objects
  // Connect as the transport does at boot: on_connected() reads at once
  void read_all_() {
    this->bus_.connected = false;
    this->bus_.establish_connection();
    ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
    this->reply_(REPLY_IDENTITY_ALPHA3);
    ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
    this->reply_(REPLY_PROCESS);
    ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
    this->reply_(REPLY_MOTOR);
  }

  TestBus bus_;
  GrundfosAlpha pump_;
  sensor::Sensor flow_, head_, medium_temperature_, voltage_, current_, power_, speed_;
  uint32_t now_{1000};
};

}  // namespace esphome::grundfos_alpha::testing
