#pragma once

#include "api_pb2_defines.h"
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

/// Count number of varints in a packed buffer
inline uint16_t count_packed_varints(const uint8_t *data, size_t len) {
  uint16_t count = 0;
  while (len > 0) {
    // Skip varint bytes until we find one without continuation bit
    while (len > 0 && (*data & 0x80)) {
      data++;
      len--;
    }
    if (len > 0) {
      data++;
      len--;
      count++;
    }
  }
  return count;
}

/// Encode a varint directly into a pre-allocated buffer.
/// Caller must ensure buffer has space (use ProtoSize::varint() to calculate).
inline void encode_varint_to_buffer(uint32_t val, uint8_t *buffer) {
  while (val > 0x7F) {
    *buffer++ = static_cast<uint8_t>(val | 0x80);
    val >>= 7;
  }
  *buffer = static_cast<uint8_t>(val);
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
 *    msg.field = StringRef(temp);
 *    return this->send_message(msg);  // temp is valid during encoding
 *
 * Unsafe Patterns (WILL cause crashes/corruption):
 * 1. Temporaries: msg.field = StringRef(obj.get_string()) // get_string() returns by value
 * 2. Concatenation: msg.field = StringRef(str1 + str2) // Result is temporary
 *
 * For unsafe patterns, store in a local variable first:
 *    std::string temp = get_string();  // or str1 + str2
 *    msg.field = StringRef(temp);
 *
 * The send_*_response pattern ensures proper lifetime management by encoding
 * within the same function scope where temporaries are created.
 */

/// Representation of a VarInt - in ProtoBuf should be 64bit but we only use 32bit
class ProtoVarInt {
 public:
  ProtoVarInt() : value_(0) {}
  explicit ProtoVarInt(uint64_t value) : value_(value) {}

  /// Parse a varint from buffer. consumed must be a valid pointer (not null).
  static optional<ProtoVarInt> parse(const uint8_t *buffer, uint32_t len, uint32_t *consumed) {
#ifdef ESPHOME_DEBUG_API
    assert(consumed != nullptr);
#endif
    if (len == 0)
      return {};
    // Fast path: single-byte varints (0-127) are the most common case
    // (booleans, small enums, field tags). Avoid loop overhead entirely.
    if ((buffer[0] & 0x80) == 0) {
      *consumed = 1;
      return ProtoVarInt(buffer[0]);
    }
    // 32-bit phase: process remaining bytes with native 32-bit shifts.
    // Without USE_API_VARINT64: cover bytes 1-4 (shifts 7, 14, 21, 28) — the uint32_t
    // shift at byte 4 (shift by 28) may lose bits 32-34, but those are always zero for valid uint32 values.
    // With USE_API_VARINT64: cover bytes 1-3 (shifts 7, 14, 21) so parse_wide handles
    // byte 4+ with full 64-bit arithmetic (avoids truncating values > UINT32_MAX).
    uint32_t result32 = buffer[0] & 0x7F;
#ifdef USE_API_VARINT64
    uint32_t limit = std::min(len, uint32_t(4));
#else
    uint32_t limit = std::min(len, uint32_t(5));
#endif
    for (uint32_t i = 1; i < limit; i++) {
      uint8_t val = buffer[i];
      result32 |= uint32_t(val & 0x7F) << (i * 7);
      if ((val & 0x80) == 0) {
        *consumed = i + 1;
        return ProtoVarInt(result32);
      }
    }
    // 64-bit phase for remaining bytes (BLE addresses etc.)
#ifdef USE_API_VARINT64
    return parse_wide(buffer, len, consumed, result32);
#else
    return {};
#endif
  }

#ifdef USE_API_VARINT64
 protected:
  /// Continue parsing varint bytes 4-9 with 64-bit arithmetic.
  /// Separated to keep 64-bit shift code (__ashldi3 on 32-bit platforms) out of the common path.
  static optional<ProtoVarInt> parse_wide(const uint8_t *buffer, uint32_t len, uint32_t *consumed, uint32_t result32)
      __attribute__((noinline));

 public:
#endif

  constexpr uint16_t as_uint16() const { return this->value_; }
  constexpr uint32_t as_uint32() const { return this->value_; }
  constexpr bool as_bool() const { return this->value_; }
  constexpr int32_t as_int32() const {
    // Not ZigZag encoded
    return static_cast<int32_t>(this->value_);
  }
  constexpr int32_t as_sint32() const {
    // with ZigZag encoding
    return decode_zigzag32(static_cast<uint32_t>(this->value_));
  }
#ifdef USE_API_VARINT64
  constexpr uint64_t as_uint64() const { return this->value_; }
  constexpr int64_t as_int64() const {
    // Not ZigZag encoded
    return static_cast<int64_t>(this->value_);
  }
  constexpr int64_t as_sint64() const {
    // with ZigZag encoding
    return decode_zigzag64(this->value_);
  }
#endif

 protected:
#ifdef USE_API_VARINT64
  uint64_t value_;
#else
  uint32_t value_;
#endif
};

// Forward declarations for decode_to_message, encode_message and encode_packed_sint32
class ProtoDecodableMessage;
class ProtoMessage;
class ProtoSize;

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
  ProtoWriteBuffer(std::vector<uint8_t> *buffer) : buffer_(buffer), pos_(buffer->data() + buffer->size()) {}
  ProtoWriteBuffer(std::vector<uint8_t> *buffer, size_t write_pos)
      : buffer_(buffer), pos_(buffer->data() + write_pos) {}
  void encode_varint_raw(uint32_t value) {
    while (value > 0x7F) {
      this->debug_check_bounds_(1);
      *this->pos_++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    }
    this->debug_check_bounds_(1);
    *this->pos_++ = static_cast<uint8_t>(value);
  }
  void encode_varint_raw_64(uint64_t value) {
    while (value > 0x7F) {
      this->debug_check_bounds_(1);
      *this->pos_++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    }
    this->debug_check_bounds_(1);
    *this->pos_++ = static_cast<uint8_t>(value);
  }
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
  void encode_field_raw(uint32_t field_id, uint32_t type) { this->encode_varint_raw((field_id << 3) | type); }
  void encode_string(uint32_t field_id, const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;

    this->encode_field_raw(field_id, 2);  // type 2: Length-delimited string
    this->encode_varint_raw(len);
    // Direct memcpy into pre-sized buffer — avoids push_back() per-byte capacity checks
    // and vector::insert() iterator overhead. ~10-11x faster for 16-32 byte strings.
    this->debug_check_bounds_(len);
    std::memcpy(this->pos_, string, len);
    this->pos_ += len;
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
    this->encode_varint_raw_64(value);
  }
  void encode_bool(uint32_t field_id, bool value, bool force = false) {
    if (!value && !force)
      return;
    this->encode_field_raw(field_id, 0);  // type 0: Varint - bool
    this->debug_check_bounds_(1);
    *this->pos_++ = value ? 0x01 : 0x00;
  }
  // noinline: 51 call sites; inlining causes net code growth vs a single out-of-line copy
  __attribute__((noinline)) void encode_fixed32(uint32_t field_id, uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;

    this->encode_field_raw(field_id, 5);  // type 5: 32-bit fixed32
    this->debug_check_bounds_(4);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    // Protobuf fixed32 is little-endian, so direct copy works
    std::memcpy(this->pos_, &value, 4);
    this->pos_ += 4;
#else
    *this->pos_++ = (value >> 0) & 0xFF;
    *this->pos_++ = (value >> 8) & 0xFF;
    *this->pos_++ = (value >> 16) & 0xFF;
    *this->pos_++ = (value >> 24) & 0xFF;
#endif
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
  /// Encode a packed repeated sint32 field (zero-copy from vector)
  void encode_packed_sint32(uint32_t field_id, const std::vector<int32_t> &values);
  /// Encode a nested message field (force=true for repeated, false for singular)
  /// Templated so concrete message type is preserved for direct encode/calculate_size calls.
  template<typename T> void encode_message(uint32_t field_id, const T &value, bool force = true);
  // Non-template core for encode_message — all buffer work happens here
  void encode_message(uint32_t field_id, uint32_t msg_length_bytes, const void *value,
                      void (*encode_fn)(const void *, ProtoWriteBuffer &), bool force);
  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
#ifdef ESPHOME_DEBUG_API
  void debug_check_bounds_(size_t bytes, const char *caller = __builtin_FUNCTION());
  void debug_check_encode_size_(uint32_t field_id, uint32_t expected, ptrdiff_t actual);
#else
  void debug_check_bounds_([[maybe_unused]] size_t bytes) {}
#endif

  std::vector<uint8_t> *buffer_;
  uint8_t *pos_;
};

#ifdef HAS_PROTO_MESSAGE_DUMP
/**
 * Fixed-size buffer for message dumps - avoids heap allocation.
 * Sized to match the logger's default tx_buffer_size (512 bytes)
 * since anything larger gets truncated anyway.
 */
class DumpBuffer {
 public:
  // Matches default tx_buffer_size in logger component
  static constexpr size_t CAPACITY = 512;

  DumpBuffer() : pos_(0) { buf_[0] = '\0'; }

  DumpBuffer &append(const char *str) {
    if (str) {
      append_impl_(str, strlen(str));
    }
    return *this;
  }

  DumpBuffer &append(const char *str, size_t len) {
    append_impl_(str, len);
    return *this;
  }

  DumpBuffer &append(size_t n, char c) {
    size_t space = CAPACITY - 1 - pos_;
    if (n > space)
      n = space;
    if (n > 0) {
      memset(buf_ + pos_, c, n);
      pos_ += n;
      buf_[pos_] = '\0';
    }
    return *this;
  }

  const char *c_str() const { return buf_; }
  size_t size() const { return pos_; }

  /// Get writable buffer pointer for use with buf_append_printf
  char *data() { return buf_; }
  /// Get current position for use with buf_append_printf
  size_t pos() const { return pos_; }
  /// Update position after buf_append_printf call
  void set_pos(size_t pos) {
    if (pos >= CAPACITY) {
      pos_ = CAPACITY - 1;
    } else {
      pos_ = pos;
    }
    buf_[pos_] = '\0';
  }

 private:
  void append_impl_(const char *str, size_t len) {
    size_t space = CAPACITY - 1 - pos_;
    if (len > space)
      len = space;
    if (len > 0) {
      memcpy(buf_ + pos_, str, len);
      pos_ += len;
      buf_[pos_] = '\0';
    }
  }

  char buf_[CAPACITY];
  size_t pos_;
};
#endif

class ProtoMessage {
 public:
  // Non-virtual defaults for messages with no fields.
  // Concrete message classes hide these with their own implementations.
  // All call sites use templates to preserve the concrete type, so virtual
  // dispatch is not needed. This eliminates per-message vtable entries for
  // encode/calculate_size, saving ~1.3 KB of flash across all message types.
  void encode(ProtoWriteBuffer &buffer) const {}
  uint32_t calculate_size() const { return 0; }
#ifdef HAS_PROTO_MESSAGE_DUMP
  virtual const char *dump_to(DumpBuffer &out) const = 0;
  virtual const char *message_name() const { return "unknown"; }
#endif

 protected:
  // Non-virtual destructor is protected to prevent polymorphic deletion.
  ~ProtoMessage() = default;
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
  ~ProtoDecodableMessage() = default;
  virtual bool decode_varint(uint32_t field_id, ProtoVarInt value) { return false; }
  virtual bool decode_length(uint32_t field_id, ProtoLengthDelimited value) { return false; }
  virtual bool decode_32bit(uint32_t field_id, Proto32Bit value) { return false; }
  // NOTE: decode_64bit removed - wire type 1 not supported
};

class ProtoSize {
 public:
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

  // Static methods that RETURN size contribution (no ProtoSize object needed).
  // Used by generated calculate_size() methods to accumulate into a plain uint32_t register.
  static constexpr uint32_t calc_int32(uint32_t field_id_size, int32_t value) {
    return value ? field_id_size + (value < 0 ? 10 : varint(static_cast<uint32_t>(value))) : 0;
  }
  static constexpr uint32_t calc_int32_force(uint32_t field_id_size, int32_t value) {
    return field_id_size + (value < 0 ? 10 : varint(static_cast<uint32_t>(value)));
  }
  static constexpr uint32_t calc_uint32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + varint(value) : 0;
  }
  static constexpr uint32_t calc_uint32_force(uint32_t field_id_size, uint32_t value) {
    return field_id_size + varint(value);
  }
  static constexpr uint32_t calc_bool(uint32_t field_id_size, bool value) { return value ? field_id_size + 1 : 0; }
  static constexpr uint32_t calc_bool_force(uint32_t field_id_size) { return field_id_size + 1; }
  static constexpr uint32_t calc_float(uint32_t field_id_size, float value) {
    return value != 0.0f ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_fixed32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_sfixed32(uint32_t field_id_size, int32_t value) {
    return value ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_sint32(uint32_t field_id_size, int32_t value) {
    return value ? field_id_size + varint(encode_zigzag32(value)) : 0;
  }
  static constexpr uint32_t calc_sint32_force(uint32_t field_id_size, int32_t value) {
    return field_id_size + varint(encode_zigzag32(value));
  }
  static constexpr uint32_t calc_int64(uint32_t field_id_size, int64_t value) {
    return value ? field_id_size + varint(value) : 0;
  }
  static constexpr uint32_t calc_int64_force(uint32_t field_id_size, int64_t value) {
    return field_id_size + varint(value);
  }
  static constexpr uint32_t calc_uint64(uint32_t field_id_size, uint64_t value) {
    return value ? field_id_size + varint(value) : 0;
  }
  static constexpr uint32_t calc_uint64_force(uint32_t field_id_size, uint64_t value) {
    return field_id_size + varint(value);
  }
  static constexpr uint32_t calc_length(uint32_t field_id_size, size_t len) {
    return len ? field_id_size + varint(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len) : 0;
  }
  static constexpr uint32_t calc_length_force(uint32_t field_id_size, size_t len) {
    return field_id_size + varint(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len);
  }
  static constexpr uint32_t calc_sint64(uint32_t field_id_size, int64_t value) {
    return value ? field_id_size + varint(encode_zigzag64(value)) : 0;
  }
  static constexpr uint32_t calc_sint64_force(uint32_t field_id_size, int64_t value) {
    return field_id_size + varint(encode_zigzag64(value));
  }
  static constexpr uint32_t calc_fixed64(uint32_t field_id_size, uint64_t value) {
    return value ? field_id_size + 8 : 0;
  }
  static constexpr uint32_t calc_sfixed64(uint32_t field_id_size, int64_t value) {
    return value ? field_id_size + 8 : 0;
  }
  static constexpr uint32_t calc_message(uint32_t field_id_size, uint32_t nested_size) {
    return nested_size ? field_id_size + varint(nested_size) + nested_size : 0;
  }
  static constexpr uint32_t calc_message_force(uint32_t field_id_size, uint32_t nested_size) {
    return field_id_size + varint(nested_size) + nested_size;
  }
};

// Implementation of methods that depend on ProtoSize being fully defined

// Implementation of encode_packed_sint32 - must be after ProtoSize is defined
inline void ProtoWriteBuffer::encode_packed_sint32(uint32_t field_id, const std::vector<int32_t> &values) {
  if (values.empty())
    return;

  // Calculate packed size
  size_t packed_size = 0;
  for (int value : values) {
    packed_size += ProtoSize::varint(encode_zigzag32(value));
  }

  // Write tag (LENGTH_DELIMITED) + length + all zigzag-encoded values
  this->encode_field_raw(field_id, WIRE_TYPE_LENGTH_DELIMITED);
  this->encode_varint_raw(packed_size);
  for (int value : values) {
    this->encode_varint_raw(encode_zigzag32(value));
  }
}

// Encode thunk — converts void* back to concrete type for direct encode() call
template<typename T> void proto_encode_msg(const void *msg, ProtoWriteBuffer &buf) {
  static_cast<const T *>(msg)->encode(buf);
}

// Implementation of encode_message - must be after ProtoMessage is defined
template<typename T> inline void ProtoWriteBuffer::encode_message(uint32_t field_id, const T &value, bool force) {
  this->encode_message(field_id, value.calculate_size(), &value, &proto_encode_msg<T>, force);
}

// Non-template core for encode_message
inline void ProtoWriteBuffer::encode_message(uint32_t field_id, uint32_t msg_length_bytes, const void *value,
                                             void (*encode_fn)(const void *, ProtoWriteBuffer &), bool force) {
  if (msg_length_bytes == 0 && !force)
    return;
  this->encode_field_raw(field_id, 2);
  this->encode_varint_raw(msg_length_bytes);
#ifdef ESPHOME_DEBUG_API
  uint8_t *start = this->pos_;
  encode_fn(value, *this);
  if (static_cast<uint32_t>(this->pos_ - start) != msg_length_bytes)
    this->debug_check_encode_size_(field_id, msg_length_bytes, this->pos_ - start);
#else
  encode_fn(value, *this);
#endif
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
  virtual void on_no_setup_connection() = 0;
  virtual bool send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) = 0;
  virtual void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) = 0;

  // Authentication helper methods
  inline bool check_connection_setup_() {
    if (!this->is_connection_setup()) {
      this->on_no_setup_connection();
      return false;
    }
    return true;
  }

  inline bool check_authenticated_() { return this->check_connection_setup_(); }
};

}  // namespace esphome::api
