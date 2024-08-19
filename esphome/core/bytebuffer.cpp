#include "bytebuffer.h"
#include <cassert>
#include <cstring>

namespace esphome {

ByteBuffer ByteBuffer::create(size_t capacity, Endian endianness) {
  std::vector<uint8_t> data(capacity);
  ByteBuffer buffer = {data};
  buffer.endianness_ = endianness;
  return buffer;
}

ByteBuffer ByteBuffer::wrap(const uint8_t *ptr, size_t len, Endian endianness) {
  std::vector<uint8_t> data(ptr, ptr + len);
  ByteBuffer buffer = {data};
  buffer.endianness_ = endianness;
  return buffer;
}

ByteBuffer ByteBuffer::wrap(std::vector<uint8_t> data, Endian endianness) {
  ByteBuffer buffer = {std::move(data)};
  buffer.endianness_ = endianness;
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint8_t value) {
  ByteBuffer buffer = ByteBuffer::create(1);
  buffer.put_uint8(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint16_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer::create(2, endianness);
  buffer.put_uint16(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint32_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer::create(4, endianness);
  buffer.put_uint32(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(uint64_t value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer::create(8, endianness);
  buffer.put_uint64(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(float value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer::create(sizeof(float), endianness);
  buffer.put_float(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(double value, Endian endianness) {
  ByteBuffer buffer = ByteBuffer::create(sizeof(double), endianness);
  buffer.put_double(value);
  return buffer;
}

ByteBuffer ByteBuffer::wrap(std::initializer_list<uint8_t> values, Endian endianness) {
  std::vector<uint8_t> buffer(values);
  return {buffer};
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
uint64_t ByteBuffer::get_uint64() {
  assert(this->get_remaining() >= 8);
  uint64_t value;
  if (endianness_ == LITTLE) {
    value = this->data_[this->position_++];
    value |= (uint64_t) this->data_[this->position_++] << 8;
    value |= (uint64_t) this->data_[this->position_++] << 16;
    value |= (uint64_t) this->data_[this->position_++] << 24;
    value |= (uint64_t) this->data_[this->position_++] << 32;
    value |= (uint64_t) this->data_[this->position_++] << 40;
    value |= (uint64_t) this->data_[this->position_++] << 48;
    value |= (uint64_t) this->data_[this->position_++] << 56;
  } else {
    value = (uint64_t) this->data_[this->position_++] << 56;
    value |= (uint64_t) this->data_[this->position_++] << 48;
    value |= (uint64_t) this->data_[this->position_++] << 40;
    value |= (uint64_t) this->data_[this->position_++] << 32;
    value |= (uint64_t) this->data_[this->position_++] << 24;
    value |= (uint64_t) this->data_[this->position_++] << 16;
    value |= (uint64_t) this->data_[this->position_++] << 8;
    value |= this->data_[this->position_++];
  }
  return value;
}
float ByteBuffer::get_float() {
  assert(this->get_remaining() >= sizeof(float));
  uint8_t byte_array[sizeof(float)];
  if (this->endianness_ == LITTLE) {
    for (uint8_t &byte_part : byte_array) {
      byte_part = this->data_[this->position_++];
    }
  } else {
    for (size_t i = sizeof(float); i > 0; i--) {
      byte_array[i - 1] = this->data_[this->position_++];
    }
  }
  float value;
  std::memcpy(&value, byte_array, sizeof(float));
  return value;
}
double ByteBuffer::get_double() {
  assert(this->get_remaining() >= sizeof(double));
  uint8_t byte_array[sizeof(double)];
  if (this->endianness_ == LITTLE) {
    for (uint8_t &byte_part : byte_array) {
      byte_part = this->data_[this->position_++];
    }
  } else {
    for (size_t i = sizeof(double); i > 0; i--) {
      byte_array[i - 1] = this->data_[this->position_++];
    }
  }
  double value;
  std::memcpy(&value, byte_array, sizeof(double));
  return value;
}

/// Putters
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
void ByteBuffer::put_uint64(uint64_t value) {
  assert(this->get_remaining() >= 8);
  if (this->endianness_ == LITTLE) {
    this->data_[this->position_++] = (uint8_t) value;
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) (value >> 16);
    this->data_[this->position_++] = (uint8_t) (value >> 24);
    this->data_[this->position_++] = (uint8_t) (value >> 32);
    this->data_[this->position_++] = (uint8_t) (value >> 40);
    this->data_[this->position_++] = (uint8_t) (value >> 48);
    this->data_[this->position_++] = (uint8_t) (value >> 56);
  } else {
    this->data_[this->position_++] = (uint8_t) (value >> 56);
    this->data_[this->position_++] = (uint8_t) (value >> 48);
    this->data_[this->position_++] = (uint8_t) (value >> 40);
    this->data_[this->position_++] = (uint8_t) (value >> 32);
    this->data_[this->position_++] = (uint8_t) (value >> 24);
    this->data_[this->position_++] = (uint8_t) (value >> 16);
    this->data_[this->position_++] = (uint8_t) (value >> 8);
    this->data_[this->position_++] = (uint8_t) value;
  }
}
void ByteBuffer::put_float(float value) {
  assert(this->get_remaining() >= sizeof(float));
  uint8_t byte_array[sizeof(float)];
  std::memcpy(byte_array, &value, sizeof(float));
  if (this->endianness_ == LITTLE) {
    for (uint8_t byte_part : byte_array) {
      this->data_[this->position_++] = byte_part;
    }
  } else {
    for (size_t i = sizeof(float); i > 0; i--) {
      this->data_[this->position_++] = byte_array[i - 1];
    }
  }
}
void ByteBuffer::put_double(double value) {
  assert(this->get_remaining() >= sizeof(double));
  uint8_t byte_array[sizeof(double)];
  std::memcpy(byte_array, &value, sizeof(double));
  if (this->endianness_ == LITTLE) {
    for (uint8_t byte_part : byte_array) {
      this->data_[this->position_++] = byte_part;
    }
  } else {
    for (size_t i = sizeof(double); i > 0; i--) {
      this->data_[this->position_++] = byte_array[i - 1];
    }
  }
}
}  // namespace esphome
