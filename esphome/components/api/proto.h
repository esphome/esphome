#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <cassert>
#include <cstring>
#include <vector>

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
#define HAS_PROTO_MESSAGE_DUMP
#endif

namespace esphome::api {

// Protocol Buffer wire type constants
// See https://protobuf.dev/programming-guides/encoding/#structure
constexpr uint8_t WIRE_TYPE_VARINT = 0;            // int32, int64, uint32, uint64, sint32, sint64, bool, enum
constexpr uint8_t WIRE_TYPE_LENGTH_DELIMITED = 2;  // string, bytes, embedded messages, packed repeated fields
constexpr uint8_t WIRE_TYPE_FIXED32 = 5;           // fixed32, sfixed32, float
constexpr uint8_t WIRE_TYPE_MASK = 0b111;          // Mask to extract wire type from tag

// Helper functions for ZigZag encoding/decoding
inline constexpr uint32_t encode_zigzag32(int32_t value) {
  return (static_cast<uint32_t>(value) << 1) ^ (static_cast<uint32_t>(value >> 31));
}

inline constexpr uint64_t encode_zigzag64(int64_t value) {
  return (static_cast<uint64_t>(value) << 1) ^ (static_cast<uint64_t>(value >> 63));
}

inline constexpr int32_t decode_zigzag32(uint32_t value) {
  return (value & 1) ? static_cast<int32_t>(~(value >> 1)) : static_cast<int32_t>(value >> 1);
}

inline constexpr int64_t decode_zigzag64(uint64_t value) {
  return (value & 1) ? static_cast<int64_t>(~(value >> 1)) : static_cast<int64_t>(value >> 1);
}

/*
 * StringRef Ownership Model for API Protocol Messages
 * ===================================================
 *
 * StringRef is used for zero-copy string handling in outgoing (SOURCE_SERVER) messages.
 * It holds a pointer and length to existing string data without copying.
 *
 * CRITICAL: The referenced string data MUST remain valid until message encoding completes.
 *
 * Safe StringRef Patterns:
 * 1. String literals: StringRef("literal") - Always safe (static storage duration)
 * 2. Member variables: StringRef(this->member_string_) - Safe if object outlives encoding
 * 3. Global/static strings: StringRef(GLOBAL_CONSTANT) - Always safe
 * 4. Local variables: Safe ONLY if encoding happens before function returns:
 *    std::string temp = compute_value();
 *    msg.set_field(StringRef(temp));
 *    return this->send_message(msg);  // temp is valid during encoding
 *
 * Unsafe Patterns (WILL cause crashes/corruption):
 * 1. Temporaries: msg.set_field(StringRef(obj.get_string())) // get_string() returns by value
 * 2. Concatenation: msg.set_field(StringRef(str1 + str2)) // Result is temporary
 *
 * For unsafe patterns, store in a local variable first:
 *    std::string temp = get_string();  // or str1 + str2
 *    msg.set_field(StringRef(temp));
 *
 * The send_*_response pattern ensures proper lifetime management by encoding
 * within the same function scope where temporaries are created.
 */

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

  constexpr uint16_t as_uint16() const { return this->value_; }
  constexpr uint32_t as_uint32() const { return this->value_; }
  constexpr uint64_t as_uint64() const { return this->value_; }
  constexpr bool as_bool() const { return this->value_; }
  constexpr int32_t as_int32() const {
    // Not ZigZag encoded
    return static_cast<int32_t>(this->as_int64());
  }
  constexpr int64_t as_int64() const {
    // Not ZigZag encoded
    return static_cast<int64_t>(this->value_);
  }
  constexpr int32_t as_sint32() const {
    // with ZigZag encoding
    return decode_zigzag32(static_cast<uint32_t>(this->value_));
  }
  constexpr int64_t as_sint64() const {
    // with ZigZag encoding
    return decode_zigzag64(this->value_);
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
class ProtoDecodableMessage;

class ProtoLengthDelimited {
 public:
  explicit ProtoLengthDelimited(const uint8_t *value, size_t length) : value_(value), length_(length) {}
  std::string as_string() const { return std::string(reinterpret_cast<const char *>(this->value_), this->length_); }

  // Direct access to raw data without string allocation
  const uint8_t *data() const { return this->value_; }
  size_t size() const { return this->length_; }

  /**
   * Decode the length-delimited data into an existing ProtoDecodableMessage instance.
   *
   * This method allows decoding without templates, enabling use in contexts
   * where the message type is not known at compile time. The ProtoDecodableMessage's
   * decode() method will be called with the raw data and length.
   *
   * @param msg The ProtoDecodableMessage instance to decode into
   */
  void decode_to_message(ProtoDecodableMessage &msg) const;

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

// NOTE: Proto64Bit class removed - wire type 1 (64-bit fixed) not supported

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
   *   - 2: Length-delimited (string, bytes, embedded messages, packed repeated fields)
   *   - 5: 32-bit (fixed32, sfixed32, float)
   *   - Note: Wire type 1 (64-bit fixed) is not supported
   *
   * Following https://protobuf.dev/programming-guides/encoding/#structure
   */
  void encode_field_raw(uint32_t field_id, uint32_t type) {
    uint32_t val = (field_id << 3) | (type & WIRE_TYPE_MASK);
    this->encode_varint_raw(val);
  }
  void encode_string(uint32_t field_id, const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;

    this->encode_field_raw(field_id, 2);  // type 2: Length-delimited string
    this->encode_varint_raw(len);

    // Using resize + memcpy instead of insert provides significant performance improvement:
    // ~10-11x faster for 16-32 byte strings, ~3x faster for 64-byte strings
    // as it avoids iterator checks and potential element moves that insert performs
    size_t old_size = this->buffer_->size();
    this->buffer_->resize(old_size + len);
    std::memcpy(this->buffer_->data() + old_size, string, len);
  }
  void encode_string(uint32_t field_id, const std::string &value, bool force = false) {
    this->encode_string(field_id, value.data(), value.size(), force);
  }
  void encode_string(uint32_t field_id, const StringRef &ref, bool force = false) {
    this->encode_string(field_id, ref.c_str(), ref.size(), force);
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
  // NOTE: Wire type 1 (64-bit fixed: double, fixed64, sfixed64) is intentionally
  // not supported to reduce overhead on embedded systems. All ESPHome devices are
  // 32-bit microcontrollers where 64-bit operations are expensive. If 64-bit support
  // is needed in the future, the necessary encoding/decoding functions must be added.
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
    this->encode_uint32(field_id, encode_zigzag32(value), force);
  }
  void encode_sint64(uint32_t field_id, int64_t value, bool force = false) {
    this->encode_uint64(field_id, encode_zigzag64(value), force);
  }
  void encode_message(uint32_t field_id, const ProtoMessage &value, bool force = false);
  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
  std::vector<uint8_t> *buffer_;
};

// Forward declaration
class ProtoSize;

class ProtoMessage {
 public:
  virtual ~ProtoMessage() = default;
  // Default implementation for messages with no fields
  virtual void encode(ProtoWriteBuffer buffer) const {}
  // Default implementation for messages with no fields
  virtual void calculate_size(ProtoSize &size) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
  std::string dump() const;
  virtual void dump_to(std::string &out) const = 0;
  virtual const char *message_name() const { return "unknown"; }
#endif
};

// Base class for messages that support decoding
class ProtoDecodableMessage : public ProtoMessage {
 public:
  virtual void decode(const uint8_t *buffer, size_t length);

  /**
   * Count occurrences of a repeated field in a protobuf buffer.
   * This is a lightweight scan that only parses tags and skips field data.
   *
   * @param buffer Pointer to the protobuf buffer
   * @param length Length of the buffer in bytes
   * @param target_field_id The field ID to count
   * @return Number of times the field appears in the buffer
   */
  static uint32_t count_repeated_field(const uint8_t *buffer, size_t length, uint32_t target_field_id);

 protected:
  virtual bool decode_varint(uint32_t field_id, ProtoVarInt value) { return false; }
  virtual bool decode_length(uint32_t field_id, ProtoLengthDelimited value) { return false; }
  virtual bool decode_32bit(uint32_t field_id, Proto32Bit value) { return false; }
  // NOTE: decode_64bit removed - wire type 1 not supported
};

class ProtoSize {
 private:
  uint32_t total_size_ = 0;

 public:
  /**
   * @brief ProtoSize class for Protocol Buffer serialization size calculation
   *
   * This class provides methods to calculate the exact byte counts needed
   * for encoding various Protocol Buffer field types. The class now uses an
   * object-based approach to reduce parameter passing overhead while keeping
   * varint calculation methods static for external use.
   *
   * Implements Protocol Buffer encoding size calculation according to:
   * https://protobuf.dev/programming-guides/encoding/
   *
   * Key features:
   * - Object-based approach reduces flash usage by eliminating parameter passing
   * - Early-return optimization for zero/default values
   * - Static varint methods for external callers
   * - Specialized handling for different field types according to protobuf spec
   */

  ProtoSize() = default;

  uint32_t get_size() const { return total_size_; }

  /**
   * @brief Calculates the size in bytes needed to encode a uint32_t value as a varint
   *
   * @param value The uint32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static constexpr uint32_t varint(uint32_t value) {
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
  static constexpr uint32_t varint(uint64_t value) {
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
  static constexpr uint32_t varint(int32_t value) {
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
  static constexpr uint32_t varint(int64_t value) {
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
  static constexpr uint32_t field(uint32_t field_id, uint32_t type) {
    uint32_t tag = (field_id << 3) | (type & WIRE_TYPE_MASK);
    return varint(tag);
  }

  /**
   * @brief Common parameters for all add_*_field methods
   *
   * All add_*_field methods follow these common patterns:
   *   * @param field_id_size Pre-calculated size of the field ID in bytes
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
  inline void add_int32(uint32_t field_id_size, int32_t value) {
    if (value != 0) {
      add_int32_force(field_id_size, value);
    }
  }

  /**
   * @brief Calculates and adds the size of an int32 field to the total message size (force version)
   */
  inline void add_int32_force(uint32_t field_id_size, int32_t value) {
    // Always calculate size when forced
    // Negative values are encoded as 10-byte varints in protobuf
    total_size_ += field_id_size + (value < 0 ? 10 : varint(static_cast<uint32_t>(value)));
  }

  /**
   * @brief Calculates and adds the size of a uint32 field to the total message size
   */
  inline void add_uint32(uint32_t field_id_size, uint32_t value) {
    if (value != 0) {
      add_uint32_force(field_id_size, value);
    }
  }

  /**
   * @brief Calculates and adds the size of a uint32 field to the total message size (force version)
   */
  inline void add_uint32_force(uint32_t field_id_size, uint32_t value) {
    // Always calculate size when force is true
    total_size_ += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a boolean field to the total message size
   */
  inline void add_bool(uint32_t field_id_size, bool value) {
    if (value) {
      // Boolean fields always use 1 byte when true
      total_size_ += field_id_size + 1;
    }
  }

  /**
   * @brief Calculates and adds the size of a boolean field to the total message size (force version)
   */
  inline void add_bool_force(uint32_t field_id_size, bool value) {
    // Always calculate size when force is true
    // Boolean fields always use 1 byte
    total_size_ += field_id_size + 1;
  }

  /**
   * @brief Calculates and adds the size of a float field to the total message size
   */
  inline void add_float(uint32_t field_id_size, float value) {
    if (value != 0.0f) {
      total_size_ += field_id_size + 4;
    }
  }

  // NOTE: add_double_field removed - wire type 1 (64-bit: double) not supported
  // to reduce overhead on embedded systems

  /**
   * @brief Calculates and adds the size of a fixed32 field to the total message size
   */
  inline void add_fixed32(uint32_t field_id_size, uint32_t value) {
    if (value != 0) {
      total_size_ += field_id_size + 4;
    }
  }

  // NOTE: add_fixed64_field removed - wire type 1 (64-bit: fixed64) not supported
  // to reduce overhead on embedded systems

  /**
   * @brief Calculates and adds the size of a sfixed32 field to the total message size
   */
  inline void add_sfixed32(uint32_t field_id_size, int32_t value) {
    if (value != 0) {
      total_size_ += field_id_size + 4;
    }
  }

  // NOTE: add_sfixed64_field removed - wire type 1 (64-bit: sfixed64) not supported
  // to reduce overhead on embedded systems

  /**
   * @brief Calculates and adds the size of a sint32 field to the total message size
   *
   * Sint32 fields use ZigZag encoding, which is more efficient for negative values.
   */
  inline void add_sint32(uint32_t field_id_size, int32_t value) {
    if (value != 0) {
      add_sint32_force(field_id_size, value);
    }
  }

  /**
   * @brief Calculates and adds the size of a sint32 field to the total message size (force version)
   *
   * Sint32 fields use ZigZag encoding, which is more efficient for negative values.
   */
  inline void add_sint32_force(uint32_t field_id_size, int32_t value) {
    // Always calculate size when force is true
    // ZigZag encoding for sint32
    total_size_ += field_id_size + varint(encode_zigzag32(value));
  }

  /**
   * @brief Calculates and adds the size of an int64 field to the total message size
   */
  inline void add_int64(uint32_t field_id_size, int64_t value) {
    if (value != 0) {
      add_int64_force(field_id_size, value);
    }
  }

  /**
   * @brief Calculates and adds the size of an int64 field to the total message size (force version)
   */
  inline void add_int64_force(uint32_t field_id_size, int64_t value) {
    // Always calculate size when force is true
    total_size_ += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a uint64 field to the total message size
   */
  inline void add_uint64(uint32_t field_id_size, uint64_t value) {
    if (value != 0) {
      add_uint64_force(field_id_size, value);
    }
  }

  /**
   * @brief Calculates and adds the size of a uint64 field to the total message size (force version)
   */
  inline void add_uint64_force(uint32_t field_id_size, uint64_t value) {
    // Always calculate size when force is true
    total_size_ += field_id_size + varint(value);
  }

  // NOTE: sint64 support functions (add_sint64_field, add_sint64_field_force) removed
  // sint64 type is not supported by ESPHome API to reduce overhead on embedded systems

  /**
   * @brief Calculates and adds the size of a length-delimited field (string/bytes) to the total message size
   */
  inline void add_length(uint32_t field_id_size, size_t len) {
    if (len != 0) {
      add_length_force(field_id_size, len);
    }
  }

  /**
   * @brief Calculates and adds the size of a length-delimited field (string/bytes) to the total message size (repeated
   * field version)
   */
  inline void add_length_force(uint32_t field_id_size, size_t len) {
    // Always calculate size when force is true
    // Field ID + length varint + data bytes
    total_size_ += field_id_size + varint(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len);
  }

  /**
   * @brief Adds a pre-calculated size directly to the total
   *
   * This is used when we can calculate the total size by multiplying the number
   * of elements by the bytes per element (for repeated fixed-size types like float, fixed32, etc.)
   *
   * @param size The pre-calculated total size to add
   */
  inline void add_precalculated_size(uint32_t size) { total_size_ += size; }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size
   *
   * This helper function directly updates the total_size reference if the nested size
   * is greater than zero.
   *
   * @param nested_size The pre-calculated size of the nested message
   */
  inline void add_message_field(uint32_t field_id_size, uint32_t nested_size) {
    if (nested_size != 0) {
      add_message_field_force(field_id_size, nested_size);
    }
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size (force version)
   *
   * @param nested_size The pre-calculated size of the nested message
   */
  inline void add_message_field_force(uint32_t field_id_size, uint32_t nested_size) {
    // Always calculate size when force is true
    // Field ID + length varint + nested message content
    total_size_ += field_id_size + varint(nested_size) + nested_size;
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
  inline void add_message_object(uint32_t field_id_size, const ProtoMessage &message) {
    // Calculate nested message size by creating a temporary ProtoSize
    ProtoSize nested_calc;
    message.calculate_size(nested_calc);
    uint32_t nested_size = nested_calc.get_size();

    // Use the base implementation with the calculated nested_size
    add_message_field(field_id_size, nested_size);
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size (force version)
   *
   * @param message The nested message object
   */
  inline void add_message_object_force(uint32_t field_id_size, const ProtoMessage &message) {
    // Calculate nested message size by creating a temporary ProtoSize
    ProtoSize nested_calc;
    message.calculate_size(nested_calc);
    uint32_t nested_size = nested_calc.get_size();

    // Use the base implementation with the calculated nested_size
    add_message_field_force(field_id_size, nested_size);
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
  inline void add_repeated_message(uint32_t field_id_size, const std::vector<MessageType> &messages) {
    // Skip if the vector is empty
    if (!messages.empty()) {
      // Use the force version for all messages in the repeated field
      for (const auto &message : messages) {
        add_message_object_force(field_id_size, message);
      }
    }
  }

  /**
   * @brief Calculates and adds the sizes of all messages in a repeated field to the total message size (FixedVector
   * version)
   *
   * @tparam MessageType The type of the nested messages in the FixedVector
   * @param messages FixedVector of message objects
   */
  template<typename MessageType>
  inline void add_repeated_message(uint32_t field_id_size, const FixedVector<MessageType> &messages) {
    // Skip if the fixed vector is empty
    if (!messages.empty()) {
      // Use the force version for all messages in the repeated field
      for (const auto &message : messages) {
        add_message_object_force(field_id_size, message);
      }
    }
  }
};

// Implementation of encode_message - must be after ProtoMessage is defined
inline void ProtoWriteBuffer::encode_message(uint32_t field_id, const ProtoMessage &value, bool force) {
  this->encode_field_raw(field_id, 2);  // type 2: Length-delimited message

  // Calculate the message size first
  ProtoSize msg_size;
  value.calculate_size(msg_size);
  uint32_t msg_length_bytes = msg_size.get_size();

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

// Implementation of decode_to_message - must be after ProtoDecodableMessage is defined
inline void ProtoLengthDelimited::decode_to_message(ProtoDecodableMessage &msg) const {
  msg.decode(this->value_, this->length_);
}

template<typename T> const char *proto_enum_to_string(T value);

class ProtoService {
 public:
 protected:
  virtual bool is_authenticated() = 0;
  virtual bool is_connection_setup() = 0;
  virtual void on_fatal_error() = 0;
#ifdef USE_API_PASSWORD
  virtual void on_unauthenticated_access() = 0;
#endif
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
  virtual void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) = 0;

  // Optimized method that pre-allocates buffer based on message size
  bool send_message_(const ProtoMessage &msg, uint8_t message_type) {
    ProtoSize size;
    msg.calculate_size(size);
    uint32_t msg_size = size.get_size();

    // Create a pre-sized buffer
    auto buffer = this->create_buffer(msg_size);

    // Encode message into the buffer
    msg.encode(buffer);

    // Send the buffer
    return this->send_buffer(buffer, message_type);
  }

  // Authentication helper methods
  inline bool check_connection_setup_() {
    if (!this->is_connection_setup()) {
      this->on_no_setup_connection();
      return false;
    }
    return true;
  }

  inline bool check_authenticated_() {
#ifdef USE_API_PASSWORD
    if (!this->check_connection_setup_()) {
      return false;
    }
    if (!this->is_authenticated()) {
      this->on_unauthenticated_access();
      return false;
    }
    return true;
#else
    return this->check_connection_setup_();
#endif
  }
};

}  // namespace esphome::api
