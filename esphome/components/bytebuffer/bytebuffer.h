#pragma once

#include <utility>
#include <vector>
#include <cinttypes>
#include <cstddef>
#include "esphome/core/helpers.h"

namespace esphome {
namespace bytebuffer {

enum Endian { LITTLE, BIG };

/**
 * A class modelled on the Java ByteBuffer class. It wraps a vector of bytes and permits putting and getting
 * items of various sizes, with an automatically incremented position.
 *
 * There are three variables maintained pointing into the buffer:
 *
 * capacity: the maximum amount of data that can be stored - set on construction and cannot be changed
 * limit: the limit of the data currently available to get or put
 * position: the current insert or extract position
 *
 * 0 <= position <= limit <= capacity
 *
 * In addition a mark can be set to the current position with mark(). A subsequent call to reset() will restore
 * the position to the mark.
 *
 * The buffer can be marked to be little-endian (default) or big-endian. All subsequent operations will use that order.
 *
 * The flip() operation will reset the position to 0 and limit to the current position. This is useful for reading
 * data from a buffer after it has been written.
 *
 * The code is defined here in the header file rather than in a .cpp file, so that it does not get compiled if not used.
 * The templated functions ensure that only those typed functions actually used are compiled. The functions
 * are implicitly inline-able which will aid performance.
 */
class ByteBuffer {
 public:
  // Default constructor (compatibility with TEMPLATABLE_VALUE)
  // Creates a zero-length ByteBuffer which is little use to anybody.
  ByteBuffer() : ByteBuffer(std::vector<uint8_t>()) {}

  /**
   * Create a new Bytebuffer with the given capacity
   */
  ByteBuffer(size_t capacity, Endian endianness = LITTLE)
      : data_(std::vector<uint8_t>(capacity)), endianness_(endianness), limit_(capacity){};

  // templated functions to implement putting and getting data of various types. There are two flavours of all
  // functions - one that uses the position as the offset, and updates the position accordingly, and one that
  // takes an explicit offset and does not update the position.
  // Separate temnplates are provided for types that fit into 32 bits and those that are bigger. These delegate
  // the actual put/get to common code based around those sizes.
  // This reduces the code size and execution time for smaller types. A similar structure for e.g. 16 bits is unlikely
  // to provide any further benefit given that all target platforms are native 32 bit.

  template<typename T>
  T get(typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    // integral types that fit into 32 bit
    return static_cast<T>(this->get_uint32_(sizeof(T)));
  }

  template<typename T>
  T get(size_t offset, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    return static_cast<T>(this->get_uint32_(offset, sizeof(T)));
  }

  template<typename T>
  void put(const T &value, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    this->put_uint32_(static_cast<uint32_t>(value), sizeof(T));
  }

  template<typename T>
  void put(const T &value, size_t offset, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    this->put_uint32_(static_cast<uint32_t>(value), offset, sizeof(T));
  }

  // integral types that do not fit into 32 bit (basically only 64 bit types)
  template<typename T>
  T get(typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    return static_cast<T>(this->get_uint64_(sizeof(T)));
  }

  template<typename T>
  T get(size_t offset, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    return static_cast<T>(this->get_uint64_(offset, sizeof(T)));
  }

  template<typename T>
  void put(const T &value, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    this->put_uint64_(value, sizeof(T));
  }

  template<typename T>
  void put(const T &value, size_t offset, typename std::enable_if<std::is_integral<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    this->put_uint64_(static_cast<uint64_t>(value), offset, sizeof(T));
  }

  // floating point types. Caters for 32 and 64 bit floating point.
  template<typename T>
  T get(typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint32_t)), T>::type * = 0) {
    return bit_cast<T>(this->get_uint32_(sizeof(T)));
  }

  template<typename T>
  T get(typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    return bit_cast<T>(this->get_uint64_(sizeof(T)));
  }

  template<typename T>
  T get(size_t offset, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint32_t)), T>::type * = 0) {
    return bit_cast<T>(this->get_uint32_(offset, sizeof(T)));
  }

  template<typename T>
  T get(size_t offset, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
        typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    return bit_cast<T>(this->get_uint64_(offset, sizeof(T)));
  }
  template<typename T>
  void put(const T &value, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    this->put_uint32_(bit_cast<uint32_t>(value), sizeof(T));
  }

  template<typename T>
  void put(const T &value, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    this->put_uint64_(bit_cast<uint64_t>(value), sizeof(T));
  }

  template<typename T>
  void put(const T &value, size_t offset, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), T>::type * = 0) {
    this->put_uint32_(bit_cast<uint32_t>(value), offset, sizeof(T));
  }

  template<typename T>
  void put(const T &value, size_t offset, typename std::enable_if<std::is_floating_point<T>::value, T>::type * = 0,
           typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), T>::type * = 0) {
    this->put_uint64_(bit_cast<uint64_t>(value), offset, sizeof(T));
  }

  template<typename T> static ByteBuffer wrap(T value, Endian endianness = LITTLE) {
    ByteBuffer buffer = ByteBuffer(sizeof(T), endianness);
    buffer.put(value);
    buffer.flip();
    return buffer;
  }

  static ByteBuffer wrap(std::vector<uint8_t> const &data, Endian endianness = LITTLE) {
    ByteBuffer buffer = {data};
    buffer.endianness_ = endianness;
    return buffer;
  }

  static ByteBuffer wrap(const uint8_t *ptr, size_t len, Endian endianness = LITTLE) {
    return wrap(std::vector<uint8_t>(ptr, ptr + len), endianness);
  }

  // convenience functions with explicit types named..
  void put_float(float value) { this->put(value); }
  void put_double(double value) { this->put(value); }

  uint8_t get_uint8() { return this->data_[this->position_++]; }
  // Get a 16 bit unsigned value, increment by 2
  uint16_t get_uint16() { return this->get<uint16_t>(); }
  // Get a 24 bit unsigned value, increment by 3
  uint32_t get_uint24() { return this->get_uint32_(3); };
  // Get a 32 bit unsigned value, increment by 4
  uint32_t get_uint32() { return this->get<uint32_t>(); };
  // Get a 64 bit unsigned value, increment by 8
  uint64_t get_uint64() { return this->get<uint64_t>(); };
  // Signed versions of the get functions
  uint8_t get_int8() { return static_cast<int8_t>(this->get_uint8()); };
  int16_t get_int16() { return this->get<uint16_t>(); }
  int32_t get_int32() { return this->get<int32_t>(); }
  int64_t get_int64() { return this->get<int64_t>(); }
  // Get a float value, increment by 4
  float get_float() { return this->get<float>(); }
  // Get a double value, increment by 8
  double get_double() { return this->get<double>(); }

  // Get a bool value, increment by 1
  bool get_bool() { return static_cast<bool>(this->get_uint8()); }

  uint32_t get_int24(size_t offset) {
    auto value = this->get_uint24(offset);
    uint32_t mask = (~static_cast<uint32_t>(0)) << 23;
    if ((value & mask) != 0)
      value |= mask;
    return value;
  }

  uint32_t get_int24() {
    auto value = this->get_uint24();
    uint32_t mask = (~static_cast<uint32_t>(0)) << 23;
    if ((value & mask) != 0)
      value |= mask;
    return value;
  }
  std::vector<uint8_t> get_vector(size_t length, size_t offset) {
    auto start = this->data_.begin() + offset;
    return {start, start + length};
  }

  std::vector<uint8_t> get_vector(size_t length) {
    auto result = this->get_vector(length, this->position_);
    this->position_ += length;
    return result;
  }

  // Convenience named functions
  void put_uint8(uint8_t value) { this->data_[this->position_++] = value; }
  void put_uint16(uint16_t value) { this->put(value); }
  void put_uint24(uint32_t value) { this->put_uint32_(value, 3); }
  void put_uint32(uint32_t value) { this->put(value); }
  void put_uint64(uint64_t value) { this->put(value); }
  // Signed versions of the put functions
  void put_int8(int8_t value) { this->put_uint8(static_cast<uint8_t>(value)); }
  void put_int16(int16_t value) { this->put(value); }
  void put_int24(int32_t value) { this->put_uint32_(value, 3); }
  void put_int32(int32_t value) { this->put(value); }
  void put_int64(int64_t value) { this->put(value); }
  // Extra put functions
  void put_bool(bool value) { this->put_uint8(value); }

  // versions of the above with an offset, these do not update the position

  uint64_t get_uint64(size_t offset) { return this->get<uint64_t>(offset); }
  uint32_t get_uint24(size_t offset) { return this->get_uint32_(offset, 3); };
  double get_double(size_t offset) { return get<double>(offset); }

  // Get one byte from the buffer, increment position by 1
  uint8_t get_uint8(size_t offset) { return this->data_[offset]; }
  // Get a 16 bit unsigned value, increment by 2
  uint16_t get_uint16(size_t offset) { return get<uint16_t>(offset); }
  // Get a 24 bit unsigned value, increment by 3
  uint32_t get_uint32(size_t offset) { return this->get<uint32_t>(offset); };
  // Get a 64 bit unsigned value, increment by 8
  uint8_t get_int8(size_t offset) { return get<int8_t>(offset); }
  int16_t get_int16(size_t offset) { return get<int16_t>(offset); }
  int32_t get_int32(size_t offset) { return get<int32_t>(offset); }
  int64_t get_int64(size_t offset) { return get<int64_t>(offset); }
  // Get a float value, increment by 4
  float get_float(size_t offset) { return get<float>(offset); }
  // Get a double value, increment by 8

  // Get a bool value, increment by 1
  bool get_bool(size_t offset) { return this->get_uint8(offset); }

  void put_uint8(uint8_t value, size_t offset) { this->data_[offset] = value; }
  void put_uint16(uint16_t value, size_t offset) { this->put(value, offset); }
  void put_uint24(uint32_t value, size_t offset) { this->put(value, offset); }
  void put_uint32(uint32_t value, size_t offset) { this->put(value, offset); }
  void put_uint64(uint64_t value, size_t offset) { this->put(value, offset); }
  // Signed versions of the put functions
  void put_int8(int8_t value, size_t offset) { this->put_uint8(static_cast<uint8_t>(value), offset); }
  void put_int16(int16_t value, size_t offset) { this->put(value, offset); }
  void put_int24(int32_t value, size_t offset) { this->put_uint32_(value, offset, 3); }
  void put_int32(int32_t value, size_t offset) { this->put(value, offset); }
  void put_int64(int64_t value, size_t offset) { this->put(value, offset); }
  // Extra put functions
  void put_float(float value, size_t offset) { this->put(value, offset); }
  void put_double(double value, size_t offset) { this->put(value, offset); }
  void put_bool(bool value, size_t offset) { this->put_uint8(value, offset); }
  void put(const std::vector<uint8_t> &value, size_t offset) {
    std::copy(value.begin(), value.end(), this->data_.begin() + offset);
  }
  void put_vector(const std::vector<uint8_t> &value, size_t offset) { this->put(value, offset); }
  void put(const std::vector<uint8_t> &value) {
    this->put_vector(value, this->position_);
    this->position_ += value.size();
  }
  void put_vector(const std::vector<uint8_t> &value) { this->put(value); }

  // Getters

  inline size_t get_capacity() const { return this->data_.size(); }
  inline size_t get_position() const { return this->position_; }
  inline size_t get_limit() const { return this->limit_; }
  inline size_t get_remaining() const { return this->get_limit() - this->get_position(); }
  inline Endian get_endianness() const { return this->endianness_; }
  inline void mark() { this->mark_ = this->position_; }
  inline void big_endian() { this->endianness_ = BIG; }
  inline void little_endian() { this->endianness_ = LITTLE; }
  // retrieve a pointer to the underlying data.
  std::vector<uint8_t> get_data() { return this->data_; };

  void get_bytes(void *dest, size_t length) {
    std::copy(this->data_.begin() + this->position_, this->data_.begin() + this->position_ + length, (uint8_t *) dest);
    this->position_ += length;
  }

  void get_bytes(void *dest, size_t length, size_t offset) {
    std::copy(this->data_.begin() + offset, this->data_.begin() + offset + length, (uint8_t *) dest);
  }

  void rewind() { this->position_ = 0; }
  void reset() { this->position_ = this->mark_; }

  void set_limit(size_t limit) { this->limit_ = limit; }
  void set_position(size_t position) { this->position_ = position; }
  void clear() {
    this->limit_ = this->get_capacity();
    this->position_ = 0;
  }
  void flip() {
    this->limit_ = this->position_;
    this->position_ = 0;
  }

 protected:
  uint64_t get_uint64_(size_t offset, size_t length) const {
    uint64_t value = 0;
    if (this->endianness_ == LITTLE) {
      offset += length;
      while (length-- != 0) {
        value <<= 8;
        value |= this->data_[--offset];
      }
    } else {
      while (length-- != 0) {
        value <<= 8;
        value |= this->data_[offset++];
      }
    }
    return value;
  }

  uint64_t get_uint64_(size_t length) {
    auto result = this->get_uint64_(this->position_, length);
    this->position_ += length;
    return result;
  }
  uint32_t get_uint32_(size_t offset, size_t length) const {
    uint32_t value = 0;
    if (this->endianness_ == LITTLE) {
      offset += length;
      while (length-- != 0) {
        value <<= 8;
        value |= this->data_[--offset];
      }
    } else {
      while (length-- != 0) {
        value <<= 8;
        value |= this->data_[offset++];
      }
    }
    return value;
  }

  uint32_t get_uint32_(size_t length) {
    auto result = this->get_uint32_(this->position_, length);
    this->position_ += length;
    return result;
  }

  /// Putters

  void put_uint64_(uint64_t value, size_t length) {
    this->put_uint64_(value, this->position_, length);
    this->position_ += length;
  }
  void put_uint32_(uint32_t value, size_t length) {
    this->put_uint32_(value, this->position_, length);
    this->position_ += length;
  }

  void put_uint64_(uint64_t value, size_t offset, size_t length) {
    if (this->endianness_ == LITTLE) {
      while (length-- != 0) {
        this->data_[offset++] = static_cast<uint8_t>(value);
        value >>= 8;
      }
    } else {
      offset += length;
      while (length-- != 0) {
        this->data_[--offset] = static_cast<uint8_t>(value);
        value >>= 8;
      }
    }
  }

  void put_uint32_(uint32_t value, size_t offset, size_t length) {
    if (this->endianness_ == LITTLE) {
      while (length-- != 0) {
        this->data_[offset++] = static_cast<uint8_t>(value);
        value >>= 8;
      }
    } else {
      offset += length;
      while (length-- != 0) {
        this->data_[--offset] = static_cast<uint8_t>(value);
        value >>= 8;
      }
    }
  }
  ByteBuffer(std::vector<uint8_t> const &data) : data_(data), limit_(data.size()) {}

  std::vector<uint8_t> data_;
  Endian endianness_{LITTLE};
  size_t position_{0};
  size_t mark_{0};
  size_t limit_{0};
};

}  // namespace bytebuffer
}  // namespace esphome
