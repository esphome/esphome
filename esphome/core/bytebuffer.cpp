#include "bytebuffer.h"
#include <cassert>

namespace esphome {

ByteBuffer ByteBuffer::create(size_t capacity) {
  std::vector<uint8_t> data(capacity);
  return {data};
}

ByteBuffer ByteBuffer::wrap(uint8_t *ptr, size_t len) {
  std::vector<uint8_t> data(ptr, ptr + len);
  return {data};
}

ByteBuffer ByteBuffer::wrap(std::vector<uint8_t> data) { return {std::move(data)}; }

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

uint32_t ByteBuffer::get_uint32() {
  assert(this->get_remaining() >= 4);
  uint32_t value;
  if (endianness_ == LITTLE) {
    value = this->data_[this->position_++];
    value |= this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++] << 16;
    value |= this->data_[this->position_++] << 24;
  } else {
    value = this->data_[this->position_++] << 24;
    value |= this->data_[this->position_++] << 16;
    value |= this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++];
  }
  return value;
}
uint32_t ByteBuffer::get_uint24() {
  assert(this->get_remaining() >= 3);
  uint32_t value;
  if (endianness_ == LITTLE) {
    value = this->data_[this->position_++];
    value |= this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++] << 16;
  } else {
    value = this->data_[this->position_++] << 16;
    value |= this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++];
  }
  return value;
}
uint32_t ByteBuffer::get_int24() {
  auto value = this->get_uint24();
  uint32_t mask = (~(uint32_t) 0) << 23;
  if ((value & mask) != 0)
    value |= mask;
  return value;
}
uint8_t ByteBuffer::get_uint8() {
  assert(this->get_remaining() >= 1);
  return this->data_[this->position_++];
}
float ByteBuffer::get_float() {
  auto value = this->get_uint32();
  return *(float *) &value;
}
void ByteBuffer::put_uint8(uint8_t value) {
  assert(this->get_remaining() >= 1);
  this->data_[this->position_++] = value;
}

void ByteBuffer::put_uint16(uint16_t value) {
  assert(this->get_remaining() >= 2);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = (uint8_t) value;
    this->data_[this->position_++] = (uint8_t) (value >> 8);
  } else {
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) value;
  }
}
void ByteBuffer::put_uint24(uint32_t value) {
  assert(this->get_remaining() >= 3);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = (uint8_t) value;
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) (value >> 16);
  } else {
    this->data_[this->position_++] = (uint8_t) (value >> 16);
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) value;
  }
}
void ByteBuffer::put_uint32(uint32_t value) {
  assert(this->get_remaining() >= 4);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = (uint8_t) value;
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) (value >> 16);
    this->data_[this->position_++] = (uint8_t) (value >> 24);
  } else {
    this->data_[this->position_++] = (uint8_t) (value >> 24);
    this->data_[this->position_++] = (uint8_t) (value >> 16);
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) value;
  }
}
void ByteBuffer::put_float(float value) { this->put_uint32(*(uint32_t *) &value); }
void ByteBuffer::flip() {
  this->limit_ = this->position_;
  this->position_ = 0;
}
}  // namespace esphome
