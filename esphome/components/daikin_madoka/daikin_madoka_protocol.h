#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/helpers.h"

// Wire format helpers kept free of ESP-IDF so they can be unit tested on the host.
namespace esphome::daikin_madoka {

static constexpr uint8_t MAX_CHUNK_SIZE = 20;
// A message's leading length byte is a uint8_t, so a full message is at most 255 bytes
static constexpr uint16_t MAX_MESSAGE_SIZE = 255;
// Longest args payload is a two-point setpoint: {0x20,0x02,hi,lo,0x21,0x02,hi,lo}
static constexpr uint8_t MAX_QUERY_ARGS = 8;
// Length byte, reserved byte and two command bytes precede the args
static constexpr uint8_t MESSAGE_HEADER_SIZE = 4;

static constexpr float MIN_TEMP = 16.0f;
static constexpr float MAX_TEMP = 32.0f;
// Setpoints are sent as fixed point with 7 fractional bits
static constexpr float SETPOINT_SCALE = 128.0f;

enum class ChunkResult : uint8_t {
  CHUNK_RESULT_PARTIAL,
  CHUNK_RESULT_COMPLETE,
  CHUNK_RESULT_INVALID_LENGTH,
  CHUNK_RESULT_OUT_OF_ORDER,
  CHUNK_RESULT_TOO_LONG,
};

/// Reassembles notifications (one chunk id byte followed by payload) into a message.
/// After CHUNK_RESULT_COMPLETE, message() holds the message until reset() or the next feed().
class MessageAssembler {
 public:
  ChunkResult feed(std::span<const uint8_t> chunk) {
    if (chunk.size() < 2)
      return ChunkResult::CHUNK_RESULT_INVALID_LENGTH;
    if (this->is_complete_())
      this->reset();
    const uint8_t chunk_id = chunk[0];
    const auto payload = chunk.subspan(1);
    if (chunk_id != this->expected_chunk_id_) {
      this->reset();
      // A chunk 0 starts a new message; any other id belongs to one we did not see begin
      if (chunk_id != 0)
        return ChunkResult::CHUNK_RESULT_OUT_OF_ORDER;
    }
    const size_t old_size = this->data_.size();
    if (payload.size() > MAX_MESSAGE_SIZE - old_size) {
      this->reset();
      return ChunkResult::CHUNK_RESULT_TOO_LONG;
    }
    this->data_.resize(old_size + payload.size());
    std::copy(payload.begin(), payload.end(), this->data_.begin() + old_size);
    this->expected_chunk_id_++;
    if (this->data_.size() > this->data_[0]) {
      this->reset();
      return ChunkResult::CHUNK_RESULT_TOO_LONG;
    }
    return this->is_complete_() ? ChunkResult::CHUNK_RESULT_COMPLETE : ChunkResult::CHUNK_RESULT_PARTIAL;
  }

  std::span<const uint8_t> message() const { return this->data_; }
  uint8_t expected_chunk_id() const { return this->expected_chunk_id_; }

  void reset() {
    this->data_.clear();
    this->expected_chunk_id_ = 0;
  }

 protected:
  bool is_complete_() const { return !this->data_.empty() && this->data_.size() == this->data_[0]; }

  StaticVector<uint8_t, MAX_MESSAGE_SIZE> data_;
  uint8_t expected_chunk_id_{0};
};

/// True when the length byte matches and the header (length, reserved, function id) is present.
inline bool is_valid_message(std::span<const uint8_t> msg) {
  return msg.size() >= MESSAGE_HEADER_SIZE && msg[0] == msg.size();
}

inline uint16_t message_function_id(std::span<const uint8_t> msg) { return encode_uint16(msg[2], msg[3]); }

/// Returns the value of argument `id`, or an empty span when it is absent or the records are truncated.
inline std::span<const uint8_t> find_arg(std::span<const uint8_t> msg, uint8_t id) {
  size_t i = MESSAGE_HEADER_SIZE;
  while (i + 2 <= msg.size()) {
    const uint8_t arg_id = msg[i];
    const uint8_t len = msg[i + 1];
    i += 2;
    if (len > msg.size() - i)
      return {};
    if (arg_id == id)
      return msg.subspan(i, len);
    i += len;
  }
  return {};
}

/// Appends a setpoint record {id, 2, msb, lsb}, clamped to the supported range.
inline void append_setpoint(StaticVector<uint8_t, MAX_QUERY_ARGS> &args, uint8_t id, float temperature) {
  const auto value = static_cast<uint16_t>(std::clamp(temperature, MIN_TEMP, MAX_TEMP) * SETPOINT_SCALE);
  const auto bytes = decode_value<uint16_t>(value);
  args.push_back(id);
  args.push_back(2);
  args.push_back(bytes[0]);
  args.push_back(bytes[1]);
}

/// Decodes a two byte setpoint value; the caller checks the size.
inline float decode_setpoint(std::span<const uint8_t> value) {
  return static_cast<float>(encode_uint16(value[0], value[1])) / SETPOINT_SCALE;
}

}  // namespace esphome::daikin_madoka
