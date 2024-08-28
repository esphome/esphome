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
uint64_t ByteBuffer::get_uint(size_t length) {
  assert(this->get_remaining() >= length);
  uint64_t value = 0;
  if (this->endianness_ == LITTLE) {
    this->position_ += length;
    auto index = this->position_;
    while (length-- != 0) {
      value <<= 8;
      value |= this->data_[--index];
    }
  } else {
    while (length-- != 0) {
      value <<= 8;
      value |= this->data_[this->position_++];
    }
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
float ByteBuffer::get_float() {
  assert(this->get_remaining() >= sizeof(float));
  auto ui_value = this->get_uint32();
  float value;
  memcpy(&value, &ui_value, sizeof(float));
  return value;
}
double ByteBuffer::get_double() {
  assert(this->get_remaining() >= sizeof(double));
  auto ui_value = this->get_uint64();
  double value;
  memcpy(&value, &ui_value, sizeof(double));
  return value;
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

void ByteBuffer::put_uint(uint64_t value, size_t length) {
  assert(this->get_remaining() >= length);
  if (this->endianness_ == LITTLE) {
    while (length-- != 0) {
      this->data_[this->position_++] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  } else {
    this->position_ += length;
    auto index = this->position_;
    while (length-- != 0) {
      this->data_[--index] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  }
}
void ByteBuffer::put_float(float value) {
  static_assert(sizeof(float) == sizeof(uint32_t), "Float sizes other than 32 bit not supported");
  assert(this->get_remaining() >= sizeof(float));
  uint32_t ui_value;
  memcpy(&ui_value, &value, sizeof(float));  // this work-around required to silence compiler warnings
  this->put_uint32(ui_value);
}
void ByteBuffer::put_double(double value) {
  static_assert(sizeof(double) == sizeof(uint64_t), "Double sizes other than 64 bit not supported");
  assert(this->get_remaining() >= sizeof(double));
  uint64_t ui_value;
  memcpy(&ui_value, &value, sizeof(double));
  this->put_uint64(ui_value);
}
void ByteBuffer::put_vector(const std::vector<uint8_t> &value) {
  assert(this->get_remaining() >= value.size());
  std::copy(value.begin(), value.end(), this->data_.begin() + this->position_);
  this->position_ += value.size();
}
}  // namespace esphome
