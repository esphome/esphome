#include "bytebuffer.h"
#include <cassert>
#include <cstring>

namespace esphome {

ByteBuffer ByteBuffer::wrap(const uint8_t *ptr, size_t len, Endian endianness) {
  // there is a double copy happening here, could be optimized but at cost of clarity.
  std::vector<uint8_t> data(ptr, ptr + len);
  ByteBuffer buffer = {data};
  buffer.endianness_ = endianness;
  return buffer;
}

ByteBuffer ByteBuffer::wrap(std::vector<uint8_t> const &data, Endian endianness) {
  ByteBuffer buffer = {data};
  buffer.endianness_ = endianness;
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint8_t value) {
  ByteBuffer buffer = ByteBuffer(1);
  buffer.put_uint8(value);
  buffer.flip();
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint16_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer(2, endianness);
  buffer.put_uint16(value);
  buffer.flip();
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint32_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer(4, endianness);
  buffer.put_uint32(value);
  buffer.flip();
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint64_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer(8, endianness);
  buffer.put_uint64(value);
  buffer.flip();
  return buffer;
}

ByteBuffer ByteBuffer::wrap(float value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer(sizeof(float), endianness);
  buffer.put_float(value);
  buffer.flip();
  return buffer;
}

ByteBuffer ByteBuffer::wrap(double value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer(sizeof(double), endianness);
  buffer.put_double(value);
  buffer.flip();
  return buffer;
}

void ByteBuffer::set_limit(size_t limit) {
  assert(limit <= this->get_capacity());
  this->limit_ = limit;
}
void ByteBuffer::set_position(size_t position) {
  assert(position <= this->get_limit());
  this->position_ = position;
}
void ByteBuffer::clear() {
  this->limit_ = this->get_capacity();
  this->position_ = 0;
}
void ByteBuffer::flip() {
  this->limit_ = this->position_;
  this->position_ = 0;
}

/// Getters
uint8_t ByteBuffer::get_uint8() {
  assert(this->get_remaining() >= 1);
  return this->data_[this->position_++];
}
uint16_t ByteBuffer::get_uint16() {
  assert(this->get_remaining() >= 2);
  uint16_t value;
  if (endianness_ == LITTLE) {
    value = this->data_[this->position_++];
    value |= this->data_[this->position_++] << 8;
  } else {
    value = this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++];
  }
  return value;
}
uint32_t ByteBuffer::get_uint24() {
  assert(this->get_remaining() >= 3);
  uint32_t value;
  if (endianness_ == LITTLE) {
    value = static_cast<uint32_t>(this->data_[this->position_++]);
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 8;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 16;
  } else {
    value = static_cast<uint32_t>(this->data_[this->position_++]) << 16;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 8;
    value |= static_cast<uint32_t>(this->data_[this->position_++]);
  }
  return value;
}
uint32_t ByteBuffer::get_int24() {
  auto value = this->get_uint24();
  uint32_t mask = (~static_cast<uint32_t>(0)) << 23;
  if ((value & mask) != 0)
    value |= mask;
  return value;
}
uint32_t ByteBuffer::get_uint32() {
  assert(this->get_remaining() >= 4);
  uint32_t value;
  if (endianness_ == LITTLE) {
    value = static_cast<uint32_t>(this->data_[this->position_++]);
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 8;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 16;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 24;
  } else {
    value = static_cast<uint32_t>(this->data_[this->position_++]) << 24;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 16;
    value |= static_cast<uint32_t>(this->data_[this->position_++]) << 8;
    value |= static_cast<uint32_t>(this->data_[this->position_++]);
  }
  return value;
}
uint64_t ByteBuffer::get_uint64() {
  assert(this->get_remaining() >= 8);
  uint64_t value;
  if (endianness_ == LITTLE) {
    value = this->data_[this->position_++];
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 8;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 16;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 24;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 32;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 40;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 48;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 56;
  } else {
    value = static_cast<uint64_t>(this->data_[this->position_++]) << 56;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 48;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 40;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 32;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 24;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 16;
    value |= static_cast<uint64_t>(this->data_[this->position_++]) << 8;
    value |= this->data_[this->position_++];
  }
  return value;
}
float ByteBuffer::get_float() {
  static_assert(sizeof(float) == sizeof(uint32_t));
  assert(this->get_remaining() >= sizeof(float));
  auto value = this->get_uint32();
  return *reinterpret_cast<float *>(&value);
}
double ByteBuffer::get_double() {
  static_assert(sizeof(double) == sizeof(uint64_t));
  assert(this->get_remaining() >= sizeof(double));
  auto value = this->get_uint64();
  return *reinterpret_cast<double *>(&value);
}
std::vector<uint8_t> ByteBuffer::get_vector(size_t length) {
  assert(this->get_remaining() >= length);
  auto start = this->data_.begin() + this->position_;
  this->position_ += length;
  return {start, start + length};
}

/// Putters
void ByteBuffer::put_uint8(uint8_t value) {
  assert(this->get_remaining() >= 1);
  this->data_[this->position_++] = value;
}

void ByteBuffer::put_uint16(uint16_t value) {
  assert(this->get_remaining() >= 2);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = static_cast<uint8_t>(value);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
  } else {
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value);
  }
}
void ByteBuffer::put_uint24(uint32_t value) {
  assert(this->get_remaining() >= 3);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = static_cast<uint8_t>(value);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
  } else {
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value);
  }
}
void ByteBuffer::put_uint32(uint32_t value) {
  assert(this->get_remaining() >= 4);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = static_cast<uint8_t>(value);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 24);
  } else {
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 24);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value);
  }
}
void ByteBuffer::put_uint64(uint64_t value) {
  assert(this->get_remaining() >= 8);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = static_cast<uint8_t>(value);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 24);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 32);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 40);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 48);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 56);
  } else {
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 56);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 48);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 40);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 32);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 24);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 16);
    this->data_[this->position_++] = static_cast<uint8_t>(value >> 8);
    this->data_[this->position_++] = static_cast<uint8_t>(value);
  }
}
void ByteBuffer::put_float(float value) {
  static_assert(sizeof(float) == sizeof(uint32_t));
  assert(this->get_remaining() >= sizeof(float));
  this->put_uint32(*reinterpret_cast<uint32_t *>(&value));
}
void ByteBuffer::put_double(double value) {
  static_assert(sizeof(double) == sizeof(uint64_t));
  assert(this->get_remaining() >= sizeof(double));
  this->put_uint64(*reinterpret_cast<uint64_t *>(&value));
}
void ByteBuffer::put_vector(const std::vector<uint8_t> &value) {
  assert(this->get_remaining() >= value.size());
  std::copy(value.begin(), value.end(), this->data_.begin() + this->position_);
  this->position_ += value.size();
}
}  // namespace esphome
