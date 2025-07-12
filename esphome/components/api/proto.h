#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cassert>
#include <vector>

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
#define HAS_PROTO_MESSAGE_DUMP
#endif

namespace esphome {
namespace api {

/// Representation of a VarInt - in ProtoBuf should be 64bit but we only use 32bit
class ProtoVarInt {
 public:
  ProtoVarInt() : value_(0) {}
  explicit ProtoVarInt(uint64_t value) : value_(value) {}

  static optional<ProtoVarInt> parse(const uint8_t *buffer, uint32_t len, uint32_t *consumed) {
    if (len == 0) {
      if (consumed != nullptr)
        *consumed = 0;
      return {};
    }

    // Most common case: single-byte varint (values 0-127)
    if ((buffer[0] & 0x80) == 0) {
      if (consumed != nullptr)
        *consumed = 1;
      return ProtoVarInt(buffer[0]);
    }

    // General case for multi-byte varints
    // Since we know buffer[0]'s high bit is set, initialize with its value
    uint64_t result = buffer[0] & 0x7F;
    uint8_t bitpos = 7;

    // Start from the second byte since we've already processed the first
    for (uint32_t i = 1; i < len; i++) {
      uint8_t val = buffer[i];
      result |= uint64_t(val & 0x7F) << uint64_t(bitpos);
      bitpos += 7;
      if ((val & 0x80) == 0) {
        if (consumed != nullptr)
          *consumed = i + 1;
        return ProtoVarInt(result);
      }
    }

    if (consumed != nullptr)
      *consumed = 0;
    return {};  // Incomplete or invalid varint
  }

  uint16_t as_uint16() const { return this->value_; }
  uint32_t as_uint32() const { return this->value_; }
  uint64_t as_uint64() const { return this->value_; }
  bool as_bool() const { return this->value_; }
  int32_t as_int32() const {
    // Not ZigZag encoded
    return static_cast<int32_t>(this->as_int64());
  }
  int64_t as_int64() const {
    // Not ZigZag encoded
    return static_cast<int64_t>(this->value_);
  }
  int32_t as_sint32() const {
    // with ZigZag encoding
    if (this->value_ & 1) {
      return static_cast<int32_t>(~(this->value_ >> 1));
    } else {
      return static_cast<int32_t>(this->value_ >> 1);
    }
  }
  int64_t as_sint64() const {
    // with ZigZag encoding
    if (this->value_ & 1) {
      return static_cast<int64_t>(~(this->value_ >> 1));
    } else {
      return static_cast<int64_t>(this->value_ >> 1);
    }
  }
  /**
   * Encode the varint value to a pre-allocated buffer without bounds checking.
   *
   * @param buffer The pre-allocated buffer to write the encoded varint to
   * @param len The size of the buffer in bytes
   *
   * @note The caller is responsible for ensuring the buffer is large enough
   *       to hold the encoded value. Use ProtoSize::varint() to calculate
   *       the exact size needed before calling this method.
   * @note No bounds checking is performed for performance reasons.
   */
  void encode_to_buffer_unchecked(uint8_t *buffer, size_t len) {
    uint64_t val = this->value_;
    if (val <= 0x7F) {
      buffer[0] = val;
      return;
    }
    size_t i = 0;
    while (val && i < len) {
      uint8_t temp = val & 0x7F;
      val >>= 7;
      if (val) {
        buffer[i++] = temp | 0x80;
      } else {
        buffer[i++] = temp;
      }
    }
  }
  void encode(std::vector<uint8_t> &out) {
    uint64_t val = this->value_;
    if (val <= 0x7F) {
      out.push_back(val);
      return;
    }
    while (val) {
      uint8_t temp = val & 0x7F;
      val >>= 7;
      if (val) {
        out.push_back(temp | 0x80);
      } else {
        out.push_back(temp);
      }
    }
  }

 protected:
  uint64_t value_;
};

// Forward declaration for decode_to_message and encode_to_writer
class ProtoMessage;

class ProtoLengthDelimited {
 public:
  explicit ProtoLengthDelimited(const uint8_t *value, size_t length) : value_(value), length_(length) {}
  std::string as_string() const { return std::string(reinterpret_cast<const char *>(this->value_), this->length_); }

  /**
   * Decode the length-delimited data into an existing ProtoMessage instance.
   *
   * This method allows decoding without templates, enabling use in contexts
   * where the message type is not known at compile time. The ProtoMessage's
   * decode() method will be called with the raw data and length.
   *
   * @param msg The ProtoMessage instance to decode into
   */
  void decode_to_message(ProtoMessage &msg) const;

 protected:
  const uint8_t *const value_;
  const size_t length_;
};

class Proto32Bit {
 public:
  explicit Proto32Bit(uint32_t value) : value_(value) {}
  uint32_t as_fixed32() const { return this->value_; }
  int32_t as_sfixed32() const { return static_cast<int32_t>(this->value_); }
  float as_float() const {
    union {
      uint32_t raw;
      float value;
    } s{};
    s.raw = this->value_;
    return s.value;
  }

 protected:
  const uint32_t value_;
};

class Proto64Bit {
 public:
  explicit Proto64Bit(uint64_t value) : value_(value) {}
  uint64_t as_fixed64() const { return this->value_; }
  int64_t as_sfixed64() const { return static_cast<int64_t>(this->value_); }
  double as_double() const {
    union {
      uint64_t raw;
      double value;
    } s{};
    s.raw = this->value_;
    return s.value;
  }

 protected:
  const uint64_t value_;
};

class ProtoWriteBuffer {
 public:
  ProtoWriteBuffer(std::vector<uint8_t> *buffer) : buffer_(buffer) {}
  void write(uint8_t value) { this->buffer_->push_back(value); }
  void encode_varint_raw(ProtoVarInt value) { value.encode(*this->buffer_); }
  void encode_varint_raw(uint32_t value) { this->encode_varint_raw(ProtoVarInt(value)); }
  /**
   * Encode a field key (tag/wire type combination).
   *
   * @param field_id Field number (tag) in the protobuf message
   * @param type Wire type value:
   *   - 0: Varint (int32, int64, uint32, uint64, sint32, sint64, bool, enum)
   *   - 1: 64-bit (fixed64, sfixed64, double)
   *   - 2: Length-delimited (string, bytes, embedded messages, packed repeated fields)
   *   - 5: 32-bit (fixed32, sfixed32, float)
   *
   * Following https://protobuf.dev/programming-guides/encoding/#structure
   */
  void encode_field_raw(uint32_t field_id, uint32_t type) {
    uint32_t val = (field_id << 3) | (type & 0b111);
    this->encode_varint_raw(val);
  }
  void encode_string(uint32_t field_id, const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;

    this->encode_field_raw(field_id, 2);  // type 2: Length-delimited string
    this->encode_varint_raw(len);
    auto *data = reinterpret_cast<const uint8_t *>(string);
    this->buffer_->insert(this->buffer_->end(), data, data + len);
  }
  void encode_string(uint32_t field_id, const std::string &value, bool force = false) {
    this->encode_string(field_id, value.data(), value.size(), force);
  }
  void encode_bytes(uint32_t field_id, const uint8_t *data, size_t len, bool force = false) {
    this->encode_string(field_id, reinterpret_cast<const char *>(data), len, force);
  }
  void encode_uint32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 0);  // type 0: Varint - uint32
    this->encode_varint_raw(value);
  }
  void encode_uint64(uint32_t field_id, uint64_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    this->encode_field_raw(field_id, 0);  // type 0: Varint - uint64
    this->encode_varint_raw(ProtoVarInt(value));
  }
  void encode_bool(uint32_t field_id, bool value, bool force = false) {
    if (!value && !force)
      return;
    this->encode_field_raw(field_id, 0);  // type 0: Varint - bool
    this->write(0x01);
  }
  void encode_fixed32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;

    this->encode_field_raw(field_id, 5);  // type 5: 32-bit fixed32
    this->write((value >> 0) & 0xFF);
    this->write((value >> 8) & 0xFF);
    this->write((value >> 16) & 0xFF);
    this->write((value >> 24) & 0xFF);
  }
  void encode_fixed64(uint32_t field_id, uint64_t value, bool force = false) {
    if (value == 0 && !force)
      return;

    this->encode_field_raw(field_id, 1);  // type 1: 64-bit fixed64
    this->write((value >> 0) & 0xFF);
    this->write((value >> 8) & 0xFF);
    this->write((value >> 16) & 0xFF);
    this->write((value >> 24) & 0xFF);
    this->write((value >> 32) & 0xFF);
    this->write((value >> 40) & 0xFF);
    this->write((value >> 48) & 0xFF);
    this->write((value >> 56) & 0xFF);
  }
  void encode_float(uint32_t field_id, float value, bool force = false) {
    if (value == 0.0f && !force)
      return;

    union {
      float value;
      uint32_t raw;
    } val{};
    val.value = value;
    this->encode_fixed32(field_id, val.raw);
  }
  void encode_int32(uint32_t field_id, int32_t value, bool force = false) {
    if (value < 0) {
      // negative int32 is always 10 byte long
      this->encode_int64(field_id, value, force);
      return;
    }
    this->encode_uint32(field_id, static_cast<uint32_t>(value), force);
  }
  void encode_int64(uint32_t field_id, int64_t value, bool force = false) {
    this->encode_uint64(field_id, static_cast<uint64_t>(value), force);
  }
  void encode_sint32(uint32_t field_id, int32_t value, bool force = false) {
    uint32_t uvalue;
    if (value < 0) {
      uvalue = ~(value << 1);
    } else {
      uvalue = value << 1;
    }
    this->encode_uint32(field_id, uvalue, force);
  }
  void encode_sint64(uint32_t field_id, int64_t value, bool force = false) {
    uint64_t uvalue;
    if (value < 0) {
      uvalue = ~(value << 1);
    } else {
      uvalue = value << 1;
    }
    this->encode_uint64(field_id, uvalue, force);
  }
  void encode_message(uint32_t field_id, const ProtoMessage &value, bool force = false);
  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
  std::vector<uint8_t> *buffer_;
};

class ProtoMessage {
 public:
  virtual ~ProtoMessage() = default;
  // Default implementation for messages with no fields
  virtual void encode(ProtoWriteBuffer buffer) const {}
  void decode(const uint8_t *buffer, size_t length);
  // Default implementation for messages with no fields
  virtual void calculate_size(uint32_t &total_size) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
  std::string dump() const;
  virtual void dump_to(std::string &out) const = 0;
  virtual const char *message_name() const { return "unknown"; }
#endif

 protected:
  virtual bool decode_varint(uint32_t field_id, ProtoVarInt value) { return false; }
  virtual bool decode_length(uint32_t field_id, ProtoLengthDelimited value) { return false; }
  virtual bool decode_32bit(uint32_t field_id, Proto32Bit value) { return false; }
  virtual bool decode_64bit(uint32_t field_id, Proto64Bit value) { return false; }
};

class ProtoSize {
 public:
  /**
   * @brief ProtoSize class for Protocol Buffer serialization size calculation
   *
   * This class provides static methods to calculate the exact byte counts needed
   * for encoding various Protocol Buffer field types. All methods are designed to be
   * efficient for the common case where many fields have default values.
   *
   * Implements Protocol Buffer encoding size calculation according to:
   * https://protobuf.dev/programming-guides/encoding/
   *
   * Key features:
   * - Early-return optimization for zero/default values
   * - Direct total_size updates to avoid unnecessary additions
   * - Specialized handling for different field types according to protobuf spec
   * - Templated helpers for repeated fields and messages
   */

  /**
   * @brief Calculates the size in bytes needed to encode a uint32_t value as a varint
   *
   * @param value The uint32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(uint32_t value) {
    // Optimized varint size calculation using leading zeros
    // Each 7 bits requires one byte in the varint encoding
    if (value < 128)
      return 1;  // 7 bits, common case for small values

    // For larger values, count bytes needed based on the position of the highest bit set
    if (value < 16384) {
      return 2;  // 14 bits
    } else if (value < 2097152) {
      return 3;  // 21 bits
    } else if (value < 268435456) {
      return 4;  // 28 bits
    } else {
      return 5;  // 32 bits (maximum for uint32_t)
    }
  }

  /**
   * @brief Calculates the size in bytes needed to encode a uint64_t value as a varint
   *
   * @param value The uint64_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(uint64_t value) {
    // Handle common case of values fitting in uint32_t (vast majority of use cases)
    if (value <= UINT32_MAX) {
      return varint(static_cast<uint32_t>(value));
    }

    // For larger values, determine size based on highest bit position
    if (value < (1ULL << 35)) {
      return 5;  // 35 bits
    } else if (value < (1ULL << 42)) {
      return 6;  // 42 bits
    } else if (value < (1ULL << 49)) {
      return 7;  // 49 bits
    } else if (value < (1ULL << 56)) {
      return 8;  // 56 bits
    } else if (value < (1ULL << 63)) {
      return 9;  // 63 bits
    } else {
      return 10;  // 64 bits (maximum for uint64_t)
    }
  }

  /**
   * @brief Calculates the size in bytes needed to encode an int32_t value as a varint
   *
   * Special handling is needed for negative values, which are sign-extended to 64 bits
   * in Protocol Buffers, resulting in a 10-byte varint.
   *
   * @param value The int32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(int32_t value) {
    // Negative values are sign-extended to 64 bits in protocol buffers,
    // which always results in a 10-byte varint for negative int32
    if (value < 0) {
      return 10;  // Negative int32 is always 10 bytes long
    }
    // For non-negative values, use the uint32_t implementation
    return varint(static_cast<uint32_t>(value));
  }

  /**
   * @brief Calculates the size in bytes needed to encode an int64_t value as a varint
   *
   * @param value The int64_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(int64_t value) {
    // For int64_t, we convert to uint64_t and calculate the size
    // This works because the bit pattern determines the encoding size,
    // and we've handled negative int32 values as a special case above
    return varint(static_cast<uint64_t>(value));
  }

  /**
   * @brief Calculates the size in bytes needed to encode a field ID and wire type
   *
   * @param field_id The field identifier
   * @param type The wire type value (from the WireType enum in the protobuf spec)
   * @return The number of bytes needed to encode the field ID and wire type
   */
  static inline uint32_t field(uint32_t field_id, uint32_t type) {
    uint32_t tag = (field_id << 3) | (type & 0b111);
    return varint(tag);
  }

  /**
   * @brief Common parameters for all add_*_field methods
   *
   * All add_*_field methods follow these common patterns:
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The value to calculate size for (type varies)
   * @param force Whether to calculate size even if the value is default/zero/empty
   *
   * Each method follows this implementation pattern:
   * 1. Skip calculation if value is default (0, false, empty) and not forced
   * 2. Calculate the size based on the field's encoding rules
   * 3. Add the field_id_size + calculated value size to total_size
   */

  /**
   * @brief Calculates and adds the size of an int32 field to the total message size
   */
  static inline void add_int32_field(uint32_t &total_size, uint32_t field_id_size, int32_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    if (value < 0) {
      // Negative values are encoded as 10-byte varints in protobuf
      total_size += field_id_size + 10;
    } else {
      // For non-negative values, use the standard varint size
      total_size += field_id_size + varint(static_cast<uint32_t>(value));
    }
  }

  /**
   * @brief Calculates and adds the size of an int32 field to the total message size (repeated field version)
   */
  static inline void add_int32_field_repeated(uint32_t &total_size, uint32_t field_id_size, int32_t value) {
    // Always calculate size for repeated fields
    if (value < 0) {
      // Negative values are encoded as 10-byte varints in protobuf
      total_size += field_id_size + 10;
    } else {
      // For non-negative values, use the standard varint size
      total_size += field_id_size + varint(static_cast<uint32_t>(value));
    }
  }

  /**
   * @brief Calculates and adds the size of a uint32 field to the total message size
   */
  static inline void add_uint32_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a uint32 field to the total message size (repeated field version)
   */
  static inline void add_uint32_field_repeated(uint32_t &total_size, uint32_t field_id_size, uint32_t value) {
    // Always calculate size for repeated fields
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a boolean field to the total message size
   */
  static inline void add_bool_field(uint32_t &total_size, uint32_t field_id_size, bool value) {
    // Skip calculation if value is false
    if (!value) {
      return;  // No need to update total_size
    }

    // Boolean fields always use 1 byte when true
    total_size += field_id_size + 1;
  }

  /**
   * @brief Calculates and adds the size of a boolean field to the total message size (repeated field version)
   */
  static inline void add_bool_field_repeated(uint32_t &total_size, uint32_t field_id_size, bool value) {
    // Always calculate size for repeated fields
    // Boolean fields always use 1 byte
    total_size += field_id_size + 1;
  }

  /**
   * @brief Calculates and adds the size of a fixed field to the total message size
   *
   * Fixed fields always take exactly N bytes (4 for fixed32/float, 8 for fixed64/double).
   *
   * @tparam NumBytes The number of bytes for this fixed field (4 or 8)
   * @param is_nonzero Whether the value is non-zero
   */
  template<uint32_t NumBytes>
  static inline void add_fixed_field(uint32_t &total_size, uint32_t field_id_size, bool is_nonzero) {
    // Skip calculation if value is zero
    if (!is_nonzero) {
      return;  // No need to update total_size
    }

    // Fixed fields always take exactly NumBytes
    total_size += field_id_size + NumBytes;
  }

  /**
   * @brief Calculates and adds the size of an enum field to the total message size
   *
   * Enum fields are encoded as uint32 varints.
   */
  static inline void add_enum_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // Enums are encoded as uint32
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of an enum field to the total message size (repeated field version)
   *
   * Enum fields are encoded as uint32 varints.
   */
  static inline void add_enum_field_repeated(uint32_t &total_size, uint32_t field_id_size, uint32_t value) {
    // Always calculate size for repeated fields
    // Enums are encoded as uint32
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a sint32 field to the total message size
   *
   * Sint32 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint32_field(uint32_t &total_size, uint32_t field_id_size, int32_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // ZigZag encoding for sint32: (n << 1) ^ (n >> 31)
    uint32_t zigzag = (static_cast<uint32_t>(value) << 1) ^ (static_cast<uint32_t>(value >> 31));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of a sint32 field to the total message size (repeated field version)
   *
   * Sint32 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint32_field_repeated(uint32_t &total_size, uint32_t field_id_size, int32_t value) {
    // Always calculate size for repeated fields
    // ZigZag encoding for sint32: (n << 1) ^ (n >> 31)
    uint32_t zigzag = (static_cast<uint32_t>(value) << 1) ^ (static_cast<uint32_t>(value >> 31));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of an int64 field to the total message size
   */
  static inline void add_int64_field(uint32_t &total_size, uint32_t field_id_size, int64_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of an int64 field to the total message size (repeated field version)
   */
  static inline void add_int64_field_repeated(uint32_t &total_size, uint32_t field_id_size, int64_t value) {
    // Always calculate size for repeated fields
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a uint64 field to the total message size
   */
  static inline void add_uint64_field(uint32_t &total_size, uint32_t field_id_size, uint64_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a uint64 field to the total message size (repeated field version)
   */
  static inline void add_uint64_field_repeated(uint32_t &total_size, uint32_t field_id_size, uint64_t value) {
    // Always calculate size for repeated fields
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a sint64 field to the total message size
   *
   * Sint64 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint64_field(uint32_t &total_size, uint32_t field_id_size, int64_t value) {
    // Skip calculation if value is zero
    if (value == 0) {
      return;  // No need to update total_size
    }

    // ZigZag encoding for sint64: (n << 1) ^ (n >> 63)
    uint64_t zigzag = (static_cast<uint64_t>(value) << 1) ^ (static_cast<uint64_t>(value >> 63));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of a sint64 field to the total message size (repeated field version)
   *
   * Sint64 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint64_field_repeated(uint32_t &total_size, uint32_t field_id_size, int64_t value) {
    // Always calculate size for repeated fields
    // ZigZag encoding for sint64: (n << 1) ^ (n >> 63)
    uint64_t zigzag = (static_cast<uint64_t>(value) << 1) ^ (static_cast<uint64_t>(value >> 63));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of a string/bytes field to the total message size
   */
  static inline void add_string_field(uint32_t &total_size, uint32_t field_id_size, const std::string &str) {
    // Skip calculation if string is empty
    if (str.empty()) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    const uint32_t str_size = static_cast<uint32_t>(str.size());
    total_size += field_id_size + varint(str_size) + str_size;
  }

  /**
   * @brief Calculates and adds the size of a string/bytes field to the total message size (repeated field version)
   */
  static inline void add_string_field_repeated(uint32_t &total_size, uint32_t field_id_size, const std::string &str) {
    // Always calculate size for repeated fields
    const uint32_t str_size = static_cast<uint32_t>(str.size());
    total_size += field_id_size + varint(str_size) + str_size;
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size
   *
   * This helper function directly updates the total_size reference if the nested size
   * is greater than zero.
   *
   * @param nested_size The pre-calculated size of the nested message
   */
  static inline void add_message_field(uint32_t &total_size, uint32_t field_id_size, uint32_t nested_size) {
    // Skip calculation if nested message is empty
    if (nested_size == 0) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    // Field ID + length varint + nested message content
    total_size += field_id_size + varint(nested_size) + nested_size;
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size (repeated field version)
   *
   * @param nested_size The pre-calculated size of the nested message
   */
  static inline void add_message_field_repeated(uint32_t &total_size, uint32_t field_id_size, uint32_t nested_size) {
    // Always calculate size for repeated fields
    // Field ID + length varint + nested message content
    total_size += field_id_size + varint(nested_size) + nested_size;
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size
   *
   * This version takes a ProtoMessage object, calculates its size internally,
   * and updates the total_size reference. This eliminates the need for a temporary variable
   * at the call site.
   *
   * @param message The nested message object
   */
  static inline void add_message_object(uint32_t &total_size, uint32_t field_id_size, const ProtoMessage &message) {
    uint32_t nested_size = 0;
    message.calculate_size(nested_size);

    // Use the base implementation with the calculated nested_size
    add_message_field(total_size, field_id_size, nested_size);
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size (repeated field version)
   *
   * @param message The nested message object
   */
  static inline void add_message_object_repeated(uint32_t &total_size, uint32_t field_id_size,
                                                 const ProtoMessage &message) {
    uint32_t nested_size = 0;
    message.calculate_size(nested_size);

    // Use the base implementation with the calculated nested_size
    add_message_field_repeated(total_size, field_id_size, nested_size);
  }

  /**
   * @brief Calculates and adds the sizes of all messages in a repeated field to the total message size
   *
   * This helper processes a vector of message objects, calculating the size for each message
   * and adding it to the total size.
   *
   * @tparam MessageType The type of the nested messages in the vector
   * @param messages Vector of message objects
   */
  template<typename MessageType>
  static inline void add_repeated_message(uint32_t &total_size, uint32_t field_id_size,
                                          const std::vector<MessageType> &messages) {
    // Skip if the vector is empty
    if (messages.empty()) {
      return;
    }

    // Use the repeated field version for all messages
    for (const auto &message : messages) {
      add_message_object_repeated(total_size, field_id_size, message);
    }
  }
};

// Implementation of encode_message - must be after ProtoMessage is defined
inline void ProtoWriteBuffer::encode_message(uint32_t field_id, const ProtoMessage &value, bool force) {
  this->encode_field_raw(field_id, 2);  // type 2: Length-delimited message

  // Calculate the message size first
  uint32_t msg_length_bytes = 0;
  value.calculate_size(msg_length_bytes);

  // Calculate how many bytes the length varint needs
  uint32_t varint_length_bytes = ProtoSize::varint(msg_length_bytes);

  // Reserve exact space for the length varint
  size_t begin = this->buffer_->size();
  this->buffer_->resize(this->buffer_->size() + varint_length_bytes);

  // Write the length varint directly
  ProtoVarInt(msg_length_bytes).encode_to_buffer_unchecked(this->buffer_->data() + begin, varint_length_bytes);

  // Now encode the message content - it will append to the buffer
  value.encode(*this);

  // Verify that the encoded size matches what we calculated
  assert(this->buffer_->size() == begin + varint_length_bytes + msg_length_bytes);
}

// Implementation of decode_to_message - must be after ProtoMessage is defined
inline void ProtoLengthDelimited::decode_to_message(ProtoMessage &msg) const {
  msg.decode(this->value_, this->length_);
}

template<typename T> const char *proto_enum_to_string(T value);

class ProtoService {
 public:
 protected:
  virtual bool is_authenticated() = 0;
  virtual bool is_connection_setup() = 0;
  virtual void on_fatal_error() = 0;
  virtual void on_unauthenticated_access() = 0;
  virtual void on_no_setup_connection() = 0;
  /**
   * Create a buffer with a reserved size.
   * @param reserve_size The number of bytes to pre-allocate in the buffer. This is a hint
   *                     to optimize memory usage and avoid reallocations during encoding.
   *                     Implementations should aim to allocate at least this size.
   * @return A ProtoWriteBuffer object with the reserved size.
   */
  virtual ProtoWriteBuffer create_buffer(uint32_t reserve_size) = 0;
  virtual bool send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) = 0;
  virtual void read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) = 0;

  // Optimized method that pre-allocates buffer based on message size
  bool send_message_(const ProtoMessage &msg, uint8_t message_type) {
    uint32_t msg_size = 0;
    msg.calculate_size(msg_size);

    // Create a pre-sized buffer
    auto buffer = this->create_buffer(msg_size);

    // Encode message into the buffer
    msg.encode(buffer);

    // Send the buffer
    return this->send_buffer(buffer, message_type);
  }

  // Authentication helper methods
  bool check_connection_setup_() {
    if (!this->is_connection_setup()) {
      this->on_no_setup_connection();
      return false;
    }
    return true;
  }

  bool check_authenticated_() {
    if (!this->check_connection_setup_()) {
      return false;
    }
    if (!this->is_authenticated()) {
      this->on_unauthenticated_access();
      return false;
    }
    return true;
  }
};

}  // namespace api
}  // namespace esphome
