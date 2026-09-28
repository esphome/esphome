#include "protocol.h"
#include <cmath>
#include <cstring>

namespace esphome::ld6004 {
uint32_t read_u32(const uint8_t *data) {
  return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}
float read_float(const uint8_t *data) {
  const uint32_t bits = read_u32(data);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
void write_u32(uint8_t *data, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    data[i] = uint8_t(value >> (8 * i));
}
void write_float(uint8_t *data, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  write_u32(data, bits);
}
size_t encode(uint16_t id, uint16_t type, const uint8_t *data, uint16_t length, uint8_t *output) {
  output[0] = 1;
  output[1] = id >> 8;
  output[2] = id;
  output[3] = length >> 8;
  output[4] = length;
  output[5] = type >> 8;
  output[6] = type;
  output[7] = 255;
  for (size_t i = 0; i < 7; ++i)
    output[7] ^= output[i];
  if (length == 0)
    return 8;
  uint8_t sum = 255;
  for (size_t i = 0; i < length; ++i) {
    output[8 + i] = data[i];
    sum ^= data[i];
  }
  output[8 + length] = sum;
  return length + 9;
}
bool Parser::feed(uint8_t byte, uint32_t now) {
  if (this->position_ != 0 && now - this->last_byte_ > FRAME_TIMEOUT_MS) {
    this->position_ = 0;
    ++this->errors_;
  }
  this->last_byte_ = now;
  if (this->position_ == 0 && byte != 1)
    return false;
  if (this->position_ < 8) {
    this->header_[this->position_++] = byte;
    if (this->position_ != 8)
      return false;
    uint8_t sum = 0;
    for (uint8_t value : this->header_)
      sum ^= value;
    const uint16_t length = (uint16_t(this->header_[3]) << 8) | this->header_[4];
    if (sum != 255) {
      this->position_ = 0;
      ++this->errors_;
      return false;
    }
    this->frame_.id = (uint16_t(this->header_[1]) << 8) | this->header_[2];
    this->frame_.type = (uint16_t(this->header_[5]) << 8) | this->header_[6];
    this->frame_.length = length;
    if (length > MAX_PAYLOAD)
      ++this->errors_;
    this->checksum_ = 255;
    if (length == 0) {
      this->position_ = 0;
      return true;
    }
    return false;
  }
  if (this->position_ < size_t(this->frame_.length) + 8) {
    if (this->frame_.length <= MAX_PAYLOAD)
      this->frame_.data[this->position_ - 8] = byte;
    ++this->position_;
    this->checksum_ ^= byte;
    return false;
  }
  this->position_ = 0;
  if (this->frame_.length > MAX_PAYLOAD)
    return false;
  if (byte == this->checksum_)
    return true;
  ++this->errors_;
  return false;
}
bool decode_targets(const uint8_t *data, size_t length, Target *targets, size_t capacity, size_t &count) {
  if (length < 4)
    return false;
  const uint32_t size = read_u32(data);
  if (size > capacity || length != 4 + size * 20)
    return false;
  for (size_t i = 0; i < size; ++i) {
    const uint8_t *point = data + 4 + 20 * i;
    Target target{read_float(point),
                  read_float(point + 4),
                  read_float(point + 8),
                  static_cast<int32_t>(read_u32(point + 12)),
                  static_cast<int32_t>(read_u32(point + 16)),
                  true};
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
      return false;
    targets[i] = target;
  }
  count = size;
  return true;
}
bool valid_report(const Frame &frame) {
  const size_t length = frame.length;
  const uint8_t *data = frame.data.data();
  if (length > MAX_PAYLOAD)
    return false;
  if (length == 0) {
    return false;
  }
  switch (frame.type) {
    case MSG_TARGETS:
      return length >= 4 && read_u32(data) <= MAX_TARGETS && length == 4 + 20 * read_u32(data);
    case MSG_AREA_PRESENCE:
      if (length != 16)
        return false;
      for (size_t i = 0; i < 4; ++i) {
        if (read_u32(data + 4 * i) > 1)
          return false;
      }
      return true;
    case MSG_WORK_STATUS:
      return length == 1 && data[0] <= 1;
    case MSG_VERSION:
      return length == 4;
    default:
      return false;
  }
}
bool Tracker::update(const uint8_t *data, size_t length, uint32_t now) {
  std::array<Target, MAX_TARGETS> incoming{};
  size_t count;
  if (!decode_targets(data, length, incoming.data(), incoming.size(), count))
    return false;
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (incoming[i].id == incoming[j].id)
        return false;
    }
  }
  std::array<bool, MAX_TARGETS> used{};
  // Retain identities before filling holes, including when input order changes.
  for (Target &slot : this->slots_) {
    bool found = false;
    if (slot.active) {
      for (size_t i = 0; i < count; ++i) {
        if (slot.id == incoming[i].id) {
          slot = incoming[i];
          used[i] = true;
          found = true;
          break;
        }
      }
    }
    slot.active = found;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!used[i]) {
      for (Target &slot : this->slots_) {
        if (!slot.active) {
          slot = incoming[i];
          break;
        }
      }
    }
  }
  this->count_ = count;
  this->valid_ = true;
  this->updated_ = now;
  return true;
}
bool Tracker::expire(uint32_t now, uint32_t timeout) {
  if (!this->valid_ || now - this->updated_ <= timeout)
    return false;
  this->valid_ = false;
  for (Target &slot : this->slots_)
    slot.active = false;
  return true;
}
uint8_t Startup::poll(uint32_t now) {
  if (this->pending_ == 0 || this->attempts_ == MAX_ATTEMPTS ||
      (this->attempts_ != 0 && now - this->sent_at_ < COMMAND_TIMEOUT_MS))
    return 0;
  ++this->attempts_;
  this->sent_at_ = now;
  return this->pending_;
}
}  // namespace esphome::ld6004
