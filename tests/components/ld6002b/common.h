#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/ld6002b/ld6002b.h"
#include "esphome/components/ld600x/ld600x_frame.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::ld6002b::testing {

static constexpr uint16_t TYPE_CONTROL = 0x0201;
static constexpr uint16_t TYPE_SET_HOLD_DELAY = 0x0203;
static constexpr uint16_t TYPE_SET_Z_RANGE = 0x0204;
static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;
static constexpr uint16_t TYPE_REPORT_POINT_CLOUD = 0x0A08;
static constexpr uint16_t TYPE_REPORT_AREA_PRESENCE = 0x0A0A;
static constexpr uint16_t TYPE_REPORT_INTERFERENCE_AREAS = 0x0A0B;
static constexpr uint16_t TYPE_REPORT_DETECTION_AREAS = 0x0A0C;
static constexpr uint16_t TYPE_REPORT_DELAY = 0x0A0D;
static constexpr uint16_t TYPE_REPORT_SENSITIVITY = 0x0A0E;
static constexpr uint16_t TYPE_REPORT_TRIGGER = 0x0A0F;
static constexpr uint16_t TYPE_REPORT_Z_RANGE = 0x0A10;
static constexpr uint16_t TYPE_REPORT_INSTALLATION = 0x0A11;
static constexpr uint16_t TYPE_REPORT_LOW_POWER = 0x0A12;
static constexpr uint16_t TYPE_REPORT_LOW_POWER_SLEEP = 0x0A13;
static constexpr uint16_t TYPE_REPORT_WORK_MODE = 0x0A14;
static constexpr uint16_t TYPE_QUERY_VERSION = 0xFFFF;

static constexpr uint32_t CMD_POINT_CLOUD_ON = 0x06;
static constexpr uint32_t CMD_TARGET_DISPLAY_ON = 0x08;
static constexpr uint32_t CMD_SENSITIVITY_HIGH = 0x0C;
// Every frame this component sends has the top bit of its id set.
static constexpr uint16_t FIRST_COMMAND_ID = 0x8000;

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

struct Target {
  float x;
  float y;
  float z;
  int32_t dop_idx;
  int32_t cluster_id;
};

inline std::vector<uint8_t> target_report(const std::vector<Target> &targets) {
  std::vector<uint8_t> p;
  put_u32(p, targets.size());
  for (const auto &t : targets) {
    put_f32(p, t.x);
    put_f32(p, t.y);
    put_f32(p, t.z);
    put_u32(p, static_cast<uint32_t>(t.dop_idx));
    put_u32(p, static_cast<uint32_t>(t.cluster_id));
  }
  return tf_frame(TYPE_REPORT_TARGET, p);
}

// What the component is expected to put on the wire for a command
inline std::vector<uint8_t> command(uint16_t id, uint16_t type, const std::vector<uint8_t> &payload) {
  return tf_frame(type, payload, id);
}

inline std::vector<uint8_t> concat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// The generated apply/publish="before" entities forward to the hub through this pointer.
inline LD6002BComponent *&test_hub() {
  static LD6002BComponent *hub = nullptr;
  return hub;
}

inline void apply_number(number::Number *entity, float value) {
  entity->publish_state(value);
  test_hub()->set_number_value(entity, value);
}
inline void apply_select(select::Select *entity, size_t index) {
  entity->publish_state(index);
  test_hub()->set_select_value(entity, index);
}
inline void apply_switch(switch_::Switch *entity, bool state) {
  entity->publish_state(state);
  test_hub()->set_switch_state(entity, state);
}

using TestNumber = number::ApplyNumber<apply_number>;
using TestSelect = select::ApplySelect<apply_select>;
using TestSwitch = switch_::ApplySwitch<apply_switch>;

/// A hub with one entity of every kind, set up as codegen does and past its boot timeout.
class LD6002BTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = std::filesystem::temp_directory_path() / "esphome_ld6002b_test";
    std::filesystem::remove_all(this->temp_dir_);
    std::filesystem::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    host::host_preferences = &this->prefs_;
    global_preferences = &this->prefs_;
    test_hub() = &this->sut_;

    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_target_count_sensor(&this->target_count_);
    this->sut_.set_point_count_sensor(&this->point_count_);
    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
      this->sut_.set_target_x_sensor(i, &this->target_x_[i]);
      this->sut_.set_target_y_sensor(i, &this->target_y_[i]);
      this->sut_.set_target_z_sensor(i, &this->target_z_[i]);
      this->sut_.set_target_dop_idx_sensor(i, &this->target_dop_[i]);
      this->sut_.set_target_cluster_id_sensor(i, &this->target_cluster_[i]);
      this->sut_.set_target_presence_binary_sensor(i, &this->target_presence_[i]);
    }
    for (uint8_t i = 0; i < AREA_COUNT; i++)
      this->sut_.set_area_presence_binary_sensor(i, &this->area_presence_[i]);
    // Interference area 0 and detection area 1, so a report has to land on the right area
    this->sut_.set_interference_area_x_min_sensor(0, &this->interference_0_[0]);
    this->sut_.set_interference_area_x_max_sensor(0, &this->interference_0_[1]);
    this->sut_.set_interference_area_y_min_sensor(0, &this->interference_0_[2]);
    this->sut_.set_interference_area_y_max_sensor(0, &this->interference_0_[3]);
    this->sut_.set_interference_area_z_min_sensor(0, &this->interference_0_[4]);
    this->sut_.set_interference_area_z_max_sensor(0, &this->interference_0_[5]);
    this->sut_.set_detection_area_x_min_sensor(1, &this->detection_1_[0]);
    this->sut_.set_detection_area_x_max_sensor(1, &this->detection_1_[1]);
    this->sut_.set_detection_area_y_min_sensor(1, &this->detection_1_[2]);
    this->sut_.set_detection_area_y_max_sensor(1, &this->detection_1_[3]);
    this->sut_.set_detection_area_z_min_sensor(1, &this->detection_1_[4]);
    this->sut_.set_detection_area_z_max_sensor(1, &this->detection_1_[5]);
    this->sut_.set_presence_binary_sensor(&this->presence_);
    this->sut_.set_work_mode_text_sensor(&this->work_mode_);
    this->sut_.set_ota_version_text_sensor(&this->ota_version_);

    this->hold_delay_.traits.set_min_value(0);
    this->hold_delay_.traits.set_max_value(65535);
    this->sut_.set_hold_delay_number(&this->hold_delay_);
    for (auto *z : {&this->z_min_, &this->z_max_}) {
      z->traits.set_min_value(-10);
      z->traits.set_max_value(10);
    }
    this->sut_.set_z_min_number(&this->z_min_);
    this->sut_.set_z_max_number(&this->z_max_);
    this->low_power_sleep_.traits.set_min_value(0);
    this->low_power_sleep_.traits.set_max_value(60000);
    this->sut_.set_low_power_sleep_number(&this->low_power_sleep_);

    this->sensitivity_.traits.set_options({"low", "medium", "high"});
    this->sut_.set_sensitivity_select(&this->sensitivity_);
    this->trigger_speed_.traits.set_options({"slow", "medium", "fast"});
    this->sut_.set_trigger_speed_select(&this->trigger_speed_);
    this->installation_.traits.set_options({"top", "side"});
    this->sut_.set_installation_select(&this->installation_);
    this->sut_.set_low_power_switch(&this->low_power_);

    this->sensor_log(this->target_count_, this->target_count_published_);
    this->sensor_log(this->point_count_, this->point_count_published_);
    this->sensor_log(this->target_x_[0], this->target_x0_published_);

    this->sut_.setup();
    // Past setup()'s 100 ms boot timeout, which starts the streams and queues the queries
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    App.scheduler.call(millis());
  }

  void TearDown() override {
    test_hub() = nullptr;
    host::setup_preferences();
    unsetenv("ESPHOME_PREFDIR");
    std::filesystem::remove_all(this->temp_dir_);
  }

  void sensor_log(sensor::Sensor &sensor, std::vector<float> &log) {
    sensor.add_on_state_callback([&log](float state) { log.push_back(state); });
  }

  void feed(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  /// ACK every boot command in turn until the queue is empty; returns the id the next command gets.
  uint16_t drain_commands() {
    uint16_t next_id = FIRST_COMMAND_ID;
    for (int i = 0; i < 32 && this->uart_.tx.size() >= ld600x::TF_FRAME_OVERHEAD; i++) {
      const uint16_t type = encode_uint16(this->uart_.tx[5], this->uart_.tx[6]);
      this->uart_.tx.clear();
      this->feed(ack(type));
      next_id++;
    }
    EXPECT_TRUE(this->uart_.tx.empty());
    return next_id;
  }

  host::HostPreferences prefs_;
  std::filesystem::path temp_dir_;
  MockUARTComponent uart_;
  LD6002BComponent sut_;

  sensor::Sensor target_count_;
  sensor::Sensor point_count_;
  sensor::Sensor target_x_[MAX_TARGETS];
  sensor::Sensor target_y_[MAX_TARGETS];
  sensor::Sensor target_z_[MAX_TARGETS];
  sensor::Sensor target_dop_[MAX_TARGETS];
  sensor::Sensor target_cluster_[MAX_TARGETS];
  // x_min, x_max, y_min, y_max, z_min, z_max
  sensor::Sensor interference_0_[6];
  sensor::Sensor detection_1_[6];
  binary_sensor::BinarySensor presence_;
  binary_sensor::BinarySensor target_presence_[MAX_TARGETS];
  binary_sensor::BinarySensor area_presence_[AREA_COUNT];
  text_sensor::TextSensor work_mode_;
  text_sensor::TextSensor ota_version_;
  TestNumber hold_delay_;
  TestNumber z_min_;
  TestNumber z_max_;
  TestNumber low_power_sleep_;
  TestSelect sensitivity_;
  TestSelect trigger_speed_;
  TestSelect installation_;
  TestSwitch low_power_;

  std::vector<float> target_count_published_;
  std::vector<float> point_count_published_;
  std::vector<float> target_x0_published_;
};

}  // namespace esphome::ld6002b::testing
