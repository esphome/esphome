#pragma once

#include <utility>
#include <vector>
#include <cinttypes>
#include <cstddef>

namespace esphome {

enum Endian { LITTLE, BIG };

/**
 * A class modelled on the Java ByteBuffer class. It wraps a vector of bytes and permits putting and getting
 * items of various sizes, with an automatically incremented position.
 *
 * There are three variables maintained pointing into the buffer:
 *
 * 0 <= position <= limit <= capacity
 *
 * capacity: the maximum amount of data that can be stored
 * limit: the limit of the data currently available to get or put
 * position: the current insert or extract position
 *
 * In addition a mark can be set to the current position with mark(). A subsequent call to reset() will restore
 * the position to the mark.
 *
 * The buffer can be marked to be little-endian (default) or big-endian. All subsequent operations will use that order.
 *
 */
class ByteBuffer {
 public:
  // Default constructor (compatibility with TEMPLATABLE_VALUE)
  ByteBuffer() : ByteBuffer(std::vector<uint8_t>()) {}
  /**
   * Create a new Bytebuffer with the given capacity
   */
  static ByteBuffer create(size_t capacity, Endian endianness = LITTLE);
  /**
   * Wrap an existing vector in a ByteBufffer
   */
  static ByteBuffer wrap(std::vector<uint8_t> data, Endian endianness = LITTLE);
  /**
   * Wrap an existing array in a ByteBufffer
   */
  static ByteBuffer wrap(const uint8_t *ptr, size_t len, Endian endianness = LITTLE);
  // Convenience functions to create a ByteBuffer from a value
  static ByteBuffer wrap(uint8_t value);
  static ByteBuffer wrap(uint16_t value, Endian endianness = LITTLE);
  static ByteBuffer wrap(uint32_t value, Endian endianness = LITTLE);
  static ByteBuffer wrap(uint64_t value, Endian endianness = LITTLE);
  static ByteBuffer wrap(int8_t value) { return wrap((uint8_t) value); }
  static ByteBuffer wrap(int16_t value, Endian endianness = LITTLE) { return wrap((uint16_t) value, endianness); }
  static ByteBuffer wrap(int32_t value, Endian endianness = LITTLE) { return wrap((uint32_t) value, endianness); }
  static ByteBuffer wrap(int64_t value, Endian endianness = LITTLE) { return wrap((uint64_t) value, endianness); }
  static ByteBuffer wrap(float value, Endian endianness = LITTLE);
  static ByteBuffer wrap(double value, Endian endianness = LITTLE);
  static ByteBuffer wrap(bool value) { return wrap(value ? (uint8_t) 1 : (uint8_t) 0); }
  static ByteBuffer wrap(std::initializer_list<uint8_t> values, Endian endianness = LITTLE);

  // Get one byte from the buffer, increment position by 1
  uint8_t get_uint8();
  // Get a 16 bit unsigned value, increment by 2
  uint16_t get_uint16();
  // Get a 24 bit unsigned value, increment by 3
  uint32_t get_uint24();
  // Get a 32 bit unsigned value, increment by 4
  uint32_t get_uint32();
  // Get a 64 bit unsigned value, increment by 8
  uint64_t get_uint64();
  // Signed versions of the get functions
  uint8_t get_int8() { return (int8_t) this->get_uint8(); };
  int16_t get_int16() { return (int16_t) this->get_uint16(); }
  uint32_t get_int24();
  int32_t get_int32() { return (int32_t) this->get_uint32(); }
  int64_t get_int64() { return (int64_t) this->get_uint64(); }
  // Get a float value, increment by 4
  float get_float();
  // Get a double value, increment by 8
  double get_double();
  // Get a bool value, increment by 1
  bool get_bool() { return this->get_uint8() != 0; }

  // Put values into the buffer, increment the position accordingly
  void put_uint8(uint8_t value);
  void put_uint16(uint16_t value);
  void put_uint24(uint32_t value);
  void put_uint32(uint32_t value);
  void put_uint64(uint64_t value);
  // Signed versions of the put functions
  void put_int8(int8_t value) { this->put_uint8(value); }
  void put_int24(int32_t value) { this->put_uint24(value); }
  void put_int32(int32_t value) { this->put_uint32(value); }
  void put_int64(int64_t value) { this->put_uint64(value); }
  // Extra put functions
  void put_float(float value);
  void put_double(double value);
  void put_bool(bool value) { this->put_uint8(value ? 1 : 0); }

  inline size_t get_capacity() const { return this->data_.size(); }
  inline size_t get_position() const { return this->position_; }
  inline size_t get_limit() const { return this->limit_; }
  inline size_t get_remaining() const { return this->get_limit() - this->get_position(); }
  inline Endian get_endianness() const { return this->endianness_; }
  inline void mark() { this->mark_ = this->position_; }
  inline void big_endian() { this->endianness_ = BIG; }
  inline void little_endian() { this->endianness_ = LITTLE; }
  void set_limit(size_t limit);
  void set_position(size_t position);
  // set position to 0, limit to capacity.
  void clear();
  // set limit to current position, postition to zero. Used when swapping from write to read operations.
  void flip();
  // retrieve a pointer to the underlying data.
  uint8_t *array() { return this->data_.data(); };
  void rewind() { this->position_ = 0; }
  void reset() { this->position_ = this->mark_; }

 protected:
  ByteBuffer(std::vector<uint8_t> data) : data_(std::move(data)) { this->limit_ = this->get_capacity(); }
  std::vector<uint8_t> data_;
  Endian endianness_{LITTLE};
  size_t position_{0};
  size_t mark_{0};
  size_t limit_{0};
};

}  // namespace esphome
