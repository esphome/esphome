#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/helpers.h"

// Wire format helpers kept free of ESP-IDF so they can be unit tested on the host.
namespace esphome::daikin_madoka {

static constexpr uint8_t MAX_CHUNK_SIZE = 20;
// A message's leading length byte is a uint8_t, so a full message is at most 255 bytes
static constexpr uint16_t MAX_MESSAGE_SIZE = 255;
// Each notification is a chunk id byte followed by up to 19 message bytes
static constexpr uint8_t MAX_CHUNK_PAYLOAD = MAX_CHUNK_SIZE - 1;
static constexpr uint8_t MAX_CHUNKS = (MAX_MESSAGE_SIZE + MAX_CHUNK_PAYLOAD - 1) / MAX_CHUNK_PAYLOAD;
// Longest args payload is a two-point setpoint: {0x20,0x02,hi,lo,0x21,0x02,hi,lo}
static constexpr uint8_t MAX_QUERY_ARGS = 8;
// Length byte, reserved byte and two command bytes precede the args
static constexpr uint8_t MESSAGE_HEADER_SIZE = 4;
// A record length of 0xFF marks an argument with no value
static constexpr uint8_t EMPTY_ARG_LENGTH = 0xFF;

static constexpr float MIN_TEMP = 16.0f;
static constexpr float MAX_TEMP = 32.0f;
// Setpoints are sent as fixed point with 7 fractional bits
static constexpr float SETPOINT_SCALE = 128.0f;

enum class ChunkResult : uint8_t {
  CHUNK_RESULT_PARTIAL,
  CHUNK_RESULT_COMPLETE,
  CHUNK_RESULT_INVALID_LENGTH,
  CHUNK_RESULT_INVALID_ID,
  CHUNK_RESULT_TOO_LONG,
};

/// Reassembles notifications (one chunk id byte followed by payload) into a message. Chunks may
/// arrive in any order: each is kept in the slot of its id, and the message is complete once chunk 0
/// is in and the chunks from 0 on add up to its length byte.
/// After CHUNK_RESULT_COMPLETE, message() holds the message until reset() or the next feed().
class MessageAssembler {
 public:
  ChunkResult feed(std::span<const uint8_t> chunk) {
    if (chunk.size() < 2)
      return ChunkResult::CHUNK_RESULT_INVALID_LENGTH;
    if (this->message_size_ != 0)
      this->reset();
    const uint8_t chunk_id = chunk[0];
    const auto payload = chunk.subspan(1);
    if (chunk_id >= MAX_CHUNKS)
      return ChunkResult::CHUNK_RESULT_INVALID_ID;
    if (payload.size() > MAX_CHUNK_PAYLOAD) {
      this->reset();
      return ChunkResult::CHUNK_RESULT_TOO_LONG;
    }
    std::copy(payload.begin(), payload.end(), this->data_.begin() + chunk_id * MAX_CHUNK_PAYLOAD);
    this->chunk_sizes_[chunk_id] = payload.size();
    if (this->chunk_sizes_[0] == 0)
      return ChunkResult::CHUNK_RESULT_PARTIAL;

    const uint8_t length = this->data_[0];
    size_t total = this->chunk_sizes_[0];
    uint8_t count = 1;
    while (total < length && count < MAX_CHUNKS && this->chunk_sizes_[count] != 0)
      total += this->chunk_sizes_[count++];
    if (total > length) {
      this->reset();
      return ChunkResult::CHUNK_RESULT_TOO_LONG;
    }
    if (total < length)
      return ChunkResult::CHUNK_RESULT_PARTIAL;
    // Close the gaps left by short chunks; each one only moves towards the start
    size_t pos = this->chunk_sizes_[0];
    for (uint8_t i = 1; i < count; i++) {
      std::copy_n(this->data_.begin() + i * MAX_CHUNK_PAYLOAD, this->chunk_sizes_[i], this->data_.begin() + pos);
      pos += this->chunk_sizes_[i];
    }
    this->message_size_ = length;
    return ChunkResult::CHUNK_RESULT_COMPLETE;
  }

  std::span<const uint8_t> message() const { return {this->data_.data(), this->message_size_}; }

  void reset() {
    this->chunk_sizes_.fill(0);
    this->message_size_ = 0;
  }

 protected:
  std::array<uint8_t, MAX_CHUNKS * MAX_CHUNK_PAYLOAD> data_;
  // Payload bytes held for each chunk id, 0 while that chunk is missing
  std::array<uint8_t, MAX_CHUNKS> chunk_sizes_{};
  uint8_t message_size_{0};
};

/// True when the length byte matches and the header (length, reserved, function id) is present.
inline bool is_valid_message(std::span<const uint8_t> msg) {
  return msg.size() >= MESSAGE_HEADER_SIZE && msg[0] == msg.size();
}

inline uint16_t message_function_id(std::span<const uint8_t> msg) { return encode_uint16(msg[2], msg[3]); }

/// Returns the value of argument `id`, or an empty span when it is absent, has no value or the records are truncated.
inline std::span<const uint8_t> find_arg(std::span<const uint8_t> msg, uint8_t id) {
  size_t i = MESSAGE_HEADER_SIZE;
  while (i + 2 <= msg.size()) {
    const uint8_t arg_id = msg[i];
    const uint8_t len = msg[i + 1] == EMPTY_ARG_LENGTH ? 0 : msg[i + 1];
    i += 2;
    if (len > msg.size() - i)
      return {};
    if (arg_id == id)
      return msg.subspan(i, len);
    i += len;
  }
  return {};
}

using QueryArgs = StaticVector<uint8_t, MAX_QUERY_ARGS>;

/// Merges the records {id, len, value...} of `incoming` into `args`: a record whose id is already
/// present is replaced in place, a new one is appended. False, with `args` unchanged, when a record
/// does not fit or the two records of one id differ in length.
inline bool merge_args(QueryArgs &args, std::span<const uint8_t> incoming) {
  QueryArgs merged = args;
  size_t i = 0;
  while (i + 2 <= incoming.size()) {
    const uint8_t id = incoming[i];
    const uint8_t len = incoming[i + 1];
    i += 2;
    if (len > incoming.size() - i)
      return false;
    const auto value = incoming.subspan(i, len);
    i += len;
    size_t j = 0;
    while (j + 2 <= merged.size() && merged[j] != id)
      j += 2 + merged[j + 1];
    if (j + 2 <= merged.size()) {
      if (merged[j + 1] != len)
        return false;
      std::copy(value.begin(), value.end(), merged.begin() + j + 2);
      continue;
    }
    if (merged.size() + 2 + len > QueryArgs::capacity())
      return false;
    merged.push_back(id);
    merged.push_back(len);
    for (uint8_t byte : value)
      merged.push_back(byte);
  }
  args = merged;
  return true;
}

/// Appends a setpoint record {id, 2, msb, lsb}, clamped to the supported range.
inline void append_setpoint(QueryArgs &args, uint8_t id, float temperature) {
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
