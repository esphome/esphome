#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace esphome::ld6004 {
// LD6004 Serial Communication Protocol, April 2026:
// https://drive.google.com/file/d/1GJj9rrRBSRkltrmo9_o-65DsHKSwMaiU/view
constexpr uint16_t MSG_CONTROL = 0x0201;
constexpr uint16_t MSG_TARGETS = 0x0A04;
constexpr uint16_t MSG_AREA_PRESENCE = 0x0A0A;
constexpr uint16_t MSG_WORK_STATUS = 0x0A14;
constexpr uint16_t MSG_VERSION = 0xFFFF;
constexpr uint8_t CMD_ENABLE_TARGETS = 0x08;
constexpr uint32_t FRAME_TIMEOUT_MS = 250;
constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
constexpr uint32_t MAX_ATTEMPTS = 3;
constexpr size_t MAX_TARGETS = 10;
constexpr size_t MAX_PAYLOAD = 4 + 20 * MAX_TARGETS;
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
// Only actual reports stop startup requests. Receipt acknowledgements carry no state.
class Startup {
 public:
  static constexpr uint8_t TARGETS = 1;
  static constexpr uint8_t VERSION = 2;
  uint8_t poll(uint32_t now);
  void received(uint8_t reports) { this->pending_ &= ~reports; }

 protected:
  uint8_t pending_{TARGETS | VERSION};
  uint8_t attempts_{0};
  uint32_t sent_at_{0};
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
