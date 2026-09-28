#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace esphome::ld6004 {
// LD6004 Serial Communication Protocol, April 2026:
// https://drive.google.com/file/d/1GJj9rrRBSRkltrmo9_o-65DsHKSwMaiU/view
constexpr uint16_t MSG_CONTROL = 0x0201;
constexpr uint16_t MSG_SET_ZONE = 0x0202;
constexpr uint16_t MSG_SET_HOLD_DELAY = 0x0203;
constexpr uint16_t MSG_SET_Z_RANGE = 0x0204;
constexpr uint16_t MSG_SET_SLEEP_TIME = 0x0205;
constexpr uint16_t MSG_SET_DWELL_LIFETIME = 0x0206;
constexpr uint16_t MSG_SET_OUTPUT_INTERVAL = 0x0207;
constexpr uint16_t MSG_TARGETS = 0x0A04;
constexpr uint16_t MSG_AREA_PRESENCE = 0x0A0A;
constexpr uint16_t MSG_INTERFERENCE_ZONES = 0x0A0B;
constexpr uint16_t MSG_DETECTION_ZONES = 0x0A0C;
constexpr uint16_t MSG_HOLD_DELAY = 0x0A0D;
constexpr uint16_t MSG_SENSITIVITY = 0x0A0E;
constexpr uint16_t MSG_TRIGGER_SPEED = 0x0A0F;
constexpr uint16_t MSG_Z_RANGE = 0x0A10;
constexpr uint16_t MSG_INSTALLATION_MODE = 0x0A11;
constexpr uint16_t MSG_WORK_MODE = 0x0A12;
constexpr uint16_t MSG_SLEEP_TIME = 0x0A13;
constexpr uint16_t MSG_WORK_STATUS = 0x0A14;
constexpr uint16_t MSG_P20_MODE = 0x0A15;
constexpr uint16_t MSG_DWELL_ZONES = 0x0A16;
constexpr uint16_t MSG_DWELL_LIFETIME = 0x0A17;
constexpr uint16_t MSG_OUTPUT_INTERVAL = 0x0A18;
constexpr uint16_t MSG_SET_BAUD_RATE = 0x0F0F;
constexpr uint16_t MSG_VERSION = 0xFFFF;
constexpr uint8_t CMD_GENERATE_INTERFERENCE = 0x01;
constexpr uint8_t CMD_QUERY_ZONES = 0x02;
constexpr uint8_t CMD_CLEAR_INTERFERENCE = 0x03;
constexpr uint8_t CMD_RESET_DETECTION = 0x04;
constexpr uint8_t CMD_QUERY_HOLD_DELAY = 0x05;
constexpr uint8_t CMD_ENABLE_TARGETS = 0x08;
constexpr uint8_t CMD_DISABLE_TARGETS = 0x09;
constexpr uint8_t CMD_SENSITIVITY_LOW = 0x0A;
constexpr uint8_t CMD_SENSITIVITY_MEDIUM = 0x0B;
constexpr uint8_t CMD_SENSITIVITY_HIGH = 0x0C;
constexpr uint8_t CMD_QUERY_SENSITIVITY = 0x0D;
constexpr uint8_t CMD_TRIGGER_SLOW = 0x0E;
constexpr uint8_t CMD_TRIGGER_MEDIUM = 0x0F;
constexpr uint8_t CMD_TRIGGER_FAST = 0x10;
constexpr uint8_t CMD_QUERY_TRIGGER_SPEED = 0x11;
constexpr uint8_t CMD_QUERY_Z_RANGE = 0x12;
constexpr uint8_t CMD_INSTALLATION_TOP = 0x13;
constexpr uint8_t CMD_INSTALLATION_SIDE = 0x14;
constexpr uint8_t CMD_QUERY_INSTALLATION_MODE = 0x15;
constexpr uint8_t CMD_WORK_LOW_POWER = 0x16;
constexpr uint8_t CMD_WORK_NORMAL = 0x17;
constexpr uint8_t CMD_QUERY_WORK_MODE = 0x18;
constexpr uint8_t CMD_QUERY_SLEEP_TIME = 0x19;
constexpr uint8_t CMD_RESET_UNOCCUPIED = 0x1A;
constexpr uint8_t CMD_WORK_RADAR_OFF_HIGH = 0x1B;
constexpr uint8_t CMD_WORK_RADAR_OFF_LOW = 0x1C;
constexpr uint8_t CMD_P20_PRESENCE_HIGH = 0x1D;
constexpr uint8_t CMD_P20_PRESENCE_LOW = 0x1E;
constexpr uint8_t CMD_QUERY_P20_MODE = 0x1F;
constexpr uint8_t CMD_P20_CONSTANT_LOW = 0x20;
constexpr uint8_t CMD_P20_CONSTANT_HIGH = 0x21;
constexpr uint8_t CMD_P20_PULSE_LOW = 0x22;
constexpr uint8_t CMD_P20_PULSE_HIGH = 0x23;
constexpr uint8_t CMD_WORK_HIGH_REFLECTIVITY = 0x24;
constexpr uint8_t CMD_CLEAR_DWELL = 0x25;
constexpr uint8_t CMD_QUERY_DWELL_LIFETIME = 0x26;
constexpr uint8_t CMD_QUERY_OUTPUT_INTERVAL = 0x27;
constexpr uint32_t FRAME_TIMEOUT_MS = 250;
constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
constexpr uint32_t RX_SEQUENCE_TIMEOUT_MS = 2000;
constexpr uint32_t MAX_ATTEMPTS = 3;
constexpr size_t MAX_TARGETS = 10;
constexpr size_t MAX_PAYLOAD = 4 + 20 * MAX_TARGETS;
bool valid_bounds(float minimum, float maximum);
uint32_t read_u32(const uint8_t *data);
float read_float(const uint8_t *data);
void write_u32(uint8_t *data, uint32_t value);
void write_float(uint8_t *data, float value);
size_t encode(uint16_t id, uint16_t type, const uint8_t *data, uint16_t length, uint8_t *output);
struct Frame {
  uint16_t id{0};
  uint16_t type{0};
  uint16_t length{0};
  std::array<uint8_t, MAX_PAYLOAD> data{};
};
bool valid_report(const Frame &frame);
class Parser {
 public:
  bool feed(uint8_t byte, uint32_t now);
  void reset() { this->position_ = 0; }
  const Frame &frame() const { return this->frame_; }
  uint32_t errors() const { return this->errors_; }

 protected:
  Frame frame_{};
  std::array<uint8_t, 8> header_{};
  size_t position_{0};
  uint8_t checksum_{255};
  uint32_t last_byte_{0};
  uint32_t errors_{0};
};
struct Target {
  float x{0};
  float y{0};
  float z{0};
  int32_t doppler{0};
  int32_t id{0};
  bool active{false};
};
bool decode_targets(const uint8_t *data, size_t length, Target *targets, size_t capacity, size_t &count);
class Tracker {
 public:
  bool update(const uint8_t *data, size_t length, uint32_t now);
  bool expire(uint32_t now, uint32_t timeout);
  const std::array<Target, MAX_TARGETS> &slots() const { return this->slots_; }
  bool valid() const { return this->valid_; }
  size_t count() const { return this->count_; }

 protected:
  std::array<Target, MAX_TARGETS> slots_{};
  size_t count_{0};
  bool valid_{false};
  uint32_t updated_{0};
};
struct Command {
  uint16_t type{0};
  uint16_t id{0};
  uint16_t report{0};
  uint8_t query{0};
  uint8_t length{0};
  std::array<uint8_t, 28> data{};
  bool retry{true};
  uint8_t z_bound{0};
  float z_value{0};
};
bool make_number_command(uint8_t index, float value, Command &command);
bool make_select_command(uint8_t index, size_t value, Command &command);
class CommandQueue {
 public:
  bool push(const Command &command);
  bool blocked() const { return this->blocked_; }
  uint8_t status() const { return this->blocked_ ? 2 : (this->busy() ? 1 : 0); }
  void recover();
  void update_z_range(float minimum, float maximum) {
    this->z_min_ = minimum;
    this->z_max_ = maximum;
  }
  bool consume_output_ack(bool &value);
  const Command *poll(uint32_t now);
  bool reply(uint16_t id, uint16_t type, size_t length, uint32_t now);
  bool busy() const { return this->size_ != 0; }
  uint32_t failures() const { return this->failures_; }

 protected:
  void finish_();
  void block_();
  std::array<Command, 8> queue_{};
  size_t head_{0};
  size_t size_{0};
  uint16_t next_id_{0};
  uint8_t attempts_{0};
  uint32_t sent_at_{0};
  uint32_t failures_{0};
  uint16_t last_rx_id_{0};
  uint32_t last_rx_time_{0};
  bool have_rx_id_{false};
  bool uncertain_{false};
  bool blocked_{false};
  uint8_t output_ack_{0};
  float z_min_{NAN};
  float z_max_{NAN};
};
class DiagnosticCounter {
 public:
  bool changed(uint32_t value) {
    if (this->initialised_ && value == this->value_)
      return false;
    this->initialised_ = true;
    this->value_ = value;
    return true;
  }

 protected:
  uint32_t value_{0};
  bool initialised_{false};
};
}  // namespace esphome::ld6004
