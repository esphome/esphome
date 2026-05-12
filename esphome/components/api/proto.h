#pragma once

#include "api_pb2_defines.h"
#include "api_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
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

// Reinterpret float bits as uint32_t without floating-point comparison.
// Used by both encode_float() and calc_float() to ensure identical zero checks.
// Uses union type-punning which is a GCC/Clang extension (not standard C++),
// but bit_cast/memcpy don't optimize to a no-op on xtensa-gcc (ESP8266).
inline uint32_t float_to_raw(float value) {
  union {
    float f;
    uint32_t u;
  } v;
  v.f = value;
  return v.u;
}

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

/// Type used for decoded varint values - uint64_t when BLE needs 64-bit addresses, uint32_t otherwise
#ifdef USE_API_VARINT64
using proto_varint_value_t = uint64_t;
#else
using proto_varint_value_t = uint32_t;
#endif

/// Sentinel value for consumed field indicating parse failure
inline constexpr uint32_t PROTO_VARINT_PARSE_FAILED = 0;

/// Result of parsing a varint: value + number of bytes consumed.
/// consumed == PROTO_VARINT_PARSE_FAILED indicates parse failure (not enough data or invalid).
struct ProtoVarIntResult {
  proto_varint_value_t value;
  uint32_t consumed;  // PROTO_VARINT_PARSE_FAILED = parse failed

  constexpr bool has_value() const { return this->consumed != PROTO_VARINT_PARSE_FAILED; }
};

/// Static varint parsing methods for the protobuf wire format.
class ProtoVarInt {
 public:
  /// Parse a varint from buffer. Caller must ensure len >= 1.
  /// Returns result with consumed=0 on failure (truncated multi-byte varint).
  static inline ProtoVarIntResult ESPHOME_ALWAYS_INLINE parse_non_empty(const uint8_t *buffer, uint32_t len) {
#ifdef ESPHOME_DEBUG_API
    assert(len > 0);
#endif
    // Fast path: single-byte varints (0-127) are the most common case
    // (booleans, small enums, field tags, small message sizes/types).
    if ((buffer[0] & 0x80) == 0) [[likely]]
      return {buffer[0], 1};
    return parse_slow(buffer, len);
  }

  /// Parse a varint from buffer (safe for empty buffers).
  /// Returns result with consumed=0 on failure (empty buffer or truncated varint).
  static inline ProtoVarIntResult ESPHOME_ALWAYS_INLINE parse(const uint8_t *buffer, uint32_t len) {
    if (len == 0)
      return {0, PROTO_VARINT_PARSE_FAILED};
    return parse_non_empty(buffer, len);
  }

 protected:
  // Slow path for multi-byte varints (>= 128), outlined to keep fast path small
  static ProtoVarIntResult parse_slow(const uint8_t *buffer, uint32_t len) __attribute__((noinline));

#ifdef USE_API_VARINT64
  /// Continue parsing varint bytes 4-9 with 64-bit arithmetic.
  static ProtoVarIntResult parse_wide(const uint8_t *buffer, uint32_t len, uint32_t result32) __attribute__((noinline));
#endif
};

// Forward declarations for encoding helpers
class ProtoMessage;
class ProtoSize;

class ProtoLengthDelimited {
 public:
  explicit ProtoLengthDelimited(const uint8_t *value, size_t length) : value_(value), length_(length) {}
  std::string as_string() const { return std::string(reinterpret_cast<const char *>(this->value_), this->length_); }

  // Direct access to raw data without string allocation
  const uint8_t *data() const { return this->value_; }
  size_t size() const { return this->length_; }

  /// Decode the length-delimited data into a message instance.
  /// Template preserves concrete type so decode() resolves statically.
  template<typename T> void decode_to_message(T &msg) const;

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

// Debug bounds checking for proto encode functions.
// In debug mode (ESPHOME_DEBUG_API), an extra end-of-buffer pointer is threaded
// through the entire encode chain. In production, these expand to nothing.
#ifdef ESPHOME_DEBUG_API
#define PROTO_ENCODE_DEBUG_PARAM , uint8_t *proto_debug_end_
#define PROTO_ENCODE_DEBUG_ARG , proto_debug_end_
#define PROTO_ENCODE_DEBUG_INIT(buf) , (buf)->data() + (buf)->size()
#define PROTO_ENCODE_CHECK_BOUNDS(pos, n) \
  do { \
    if ((pos) + (n) > proto_debug_end_) \
      proto_check_bounds_failed(pos, n, proto_debug_end_, __builtin_FUNCTION()); \
  } while (0)
void proto_check_bounds_failed(const uint8_t *pos, size_t bytes, const uint8_t *end, const char *caller);
#else
#define PROTO_ENCODE_DEBUG_PARAM
#define PROTO_ENCODE_DEBUG_ARG
#define PROTO_ENCODE_DEBUG_INIT(buf)
#define PROTO_ENCODE_CHECK_BOUNDS(pos, n)
#endif

class ProtoWriteBuffer {
 public:
  ProtoWriteBuffer(APIBuffer *buffer) : buffer_(buffer), pos_(buffer->data() + buffer->size()) {}
  ProtoWriteBuffer(APIBuffer *buffer, size_t write_pos) : buffer_(buffer), pos_(buffer->data() + write_pos) {}
  inline void ESPHOME_ALWAYS_INLINE encode_varint_raw(uint32_t value) {
    if (value < 128) [[likely]] {
      this->debug_check_bounds_(1);
      *this->pos_++ = static_cast<uint8_t>(value);
      return;
    }
    this->encode_varint_raw_slow_(value);
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
  /// Single-pass encode for repeated submessage elements.
  /// Thin template wrapper; all buffer work is in the non-template core.
  template<typename T> void encode_sub_message(uint32_t field_id, const T &value);
  /// Encode an optional singular submessage field — skips if empty.
  /// Thin template wrapper; all buffer work is in the non-template core.
  template<typename T> void encode_optional_sub_message(uint32_t field_id, const T &value);

  // NOLINTBEGIN(readability-identifier-naming)
  // Non-template core for encode_sub_message — backpatch approach.
  void encode_sub_message(uint32_t field_id, const void *value,
                          uint8_t *(*encode_fn)(const void *, ProtoWriteBuffer &PROTO_ENCODE_DEBUG_PARAM));
  // Non-template core for encode_optional_sub_message.
  void encode_optional_sub_message(uint32_t field_id, uint32_t nested_size, const void *value,
                                   uint8_t *(*encode_fn)(const void *, ProtoWriteBuffer &PROTO_ENCODE_DEBUG_PARAM));
  // NOLINTEND(readability-identifier-naming)
  APIBuffer *get_buffer() const { return buffer_; }
  uint8_t *get_pos() const { return pos_; }
  void set_pos(uint8_t *pos) { pos_ = pos; }

 protected:
  // Slow path for encode_varint_raw values >= 128, outlined to keep fast path small
  void encode_varint_raw_slow_(uint32_t value) __attribute__((noinline));

#ifdef ESPHOME_DEBUG_API
  void debug_check_bounds_(size_t bytes, const char *caller = __builtin_FUNCTION());
  void debug_check_encode_size_(uint32_t field_id, uint32_t expected, ptrdiff_t actual);
#else
  void debug_check_bounds_([[maybe_unused]] size_t bytes) {}
#endif

  APIBuffer *buffer_;
  uint8_t *pos_;
};

// Varint encoding thresholds — used by both proto_encode_* free functions and ProtoSize.
constexpr uint32_t VARINT_MAX_1_BYTE = 1 << 7;   // 128
constexpr uint32_t VARINT_MAX_2_BYTE = 1 << 14;  // 16384

/// Static encode helpers for generated encode() functions.
/// Generated code hoists buffer.pos_ into a local uint8_t *__restrict__ pos,
/// then calls these methods which take pos by reference. No struct, no overhead.
/// For sub-messages, pos is synced back to buffer before the call and reloaded after.
class ProtoEncode {
 public:
  /// Write a multi-byte varint directly through a pos pointer.
  template<typename T>
  static inline void encode_varint_raw_loop(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, T value) {
    do {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
      *pos++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    } while (value > 0x7F);
    PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
    *pos++ = static_cast<uint8_t>(value);
  }
  static inline void ESPHOME_ALWAYS_INLINE encode_varint_raw(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                             uint32_t value) {
    if (value < VARINT_MAX_1_BYTE) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
      *pos++ = static_cast<uint8_t>(value);
      return;
    }
    encode_varint_raw_loop(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  /// Encode a varint that is expected to be 1-2 bytes (e.g. zigzag RSSI, small lengths).
  static inline void ESPHOME_ALWAYS_INLINE encode_varint_raw_short(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                                   uint32_t value) {
    if (value < VARINT_MAX_1_BYTE) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
      *pos++ = static_cast<uint8_t>(value);
      return;
    }
    if (value < VARINT_MAX_2_BYTE) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 2);
      *pos++ = static_cast<uint8_t>(value | 0x80);
      *pos++ = static_cast<uint8_t>(value >> 7);
      return;
    }
    encode_varint_raw_loop(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  static inline void ESPHOME_ALWAYS_INLINE encode_varint_raw_64(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                                uint64_t value) {
    if (value < VARINT_MAX_1_BYTE) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
      *pos++ = static_cast<uint8_t>(value);
      return;
    }
    encode_varint_raw_loop(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  /// Encode a 48-bit MAC address (stored in a uint64) as varint.
  /// Real MAC addresses occupy the full 48 bits (OUI in upper 24), so the
  /// fast path -- any non-zero bit in the top 6 of 48 -- emits exactly 7 bytes
  /// with no per-byte branch. Falls back to the general loop otherwise.
  /// Caller must guarantee value fits in 48 bits (checked in debug builds).
  static inline void ESPHOME_ALWAYS_INLINE encode_varint_raw_48bit(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                                   uint64_t value) {
#ifdef ESPHOME_DEBUG_API
    assert(value < (1ULL << (MAC_ADDRESS_SIZE * 8)) && "encode_varint_raw_48bit: value exceeds 48 bits");
#endif
    // 7-byte varint holds 49 bits (7 * 7), so a 48-bit value needs all 7 bytes
    // whenever bit 42 or higher is set (i.e. value >= 1 << (48 - 6)).
    if (value >= (1ULL << (MAC_ADDRESS_SIZE * 8 - 6))) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 7);
      pos[0] = static_cast<uint8_t>(value | 0x80);
      pos[1] = static_cast<uint8_t>((value >> 7) | 0x80);
      pos[2] = static_cast<uint8_t>((value >> 14) | 0x80);
      pos[3] = static_cast<uint8_t>((value >> 21) | 0x80);
      pos[4] = static_cast<uint8_t>((value >> 28) | 0x80);
      pos[5] = static_cast<uint8_t>((value >> 35) | 0x80);
      pos[6] = static_cast<uint8_t>(value >> 42);
      pos += 7;
      return;
    }
    encode_varint_raw_64(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  static inline void ESPHOME_ALWAYS_INLINE encode_field_raw(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                            uint32_t field_id, uint32_t type) {
    encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, (field_id << 3) | type);
  }
  /// Write a single precomputed tag byte. Tag must be < 128.
  static inline void ESPHOME_ALWAYS_INLINE write_raw_byte(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                          uint8_t b) {
    PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
    *pos++ = b;
  }
  /// Reserve one byte for later backpatch (e.g., sub-message length).
  /// Advances pos past the reserved byte without writing a value.
  static inline void ESPHOME_ALWAYS_INLINE reserve_byte(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) {
    PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
    pos++;
  }
  /// Write raw bytes to the buffer (no tag, no length prefix).
  static inline void ESPHOME_ALWAYS_INLINE encode_raw(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                      const void *data, size_t len) {
    PROTO_ENCODE_CHECK_BOUNDS(pos, len);
    std::memcpy(pos, data, len);
    pos += len;
  }
  /// Encode tag + 1-byte length + raw string data. For strings with max_data_length < 128.
  /// Tag must be a single-byte varint (< 128). Always encodes (no zero check).
  static inline void encode_short_string_force(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint8_t tag,
                                               const StringRef &ref) {
#ifdef ESPHOME_DEBUG_API
    assert(ref.size() < 128 && "encode_short_string_force: string exceeds max_data_length < 128");
#endif
    PROTO_ENCODE_CHECK_BOUNDS(pos, 2 + ref.size());
    pos[0] = tag;
    pos[1] = static_cast<uint8_t>(ref.size());
    std::memcpy(pos + 2, ref.c_str(), ref.size());
    pos += 2 + ref.size();
  }
  /// Write a precomputed tag byte + 32-bit value in one operation.
  static inline void ESPHOME_ALWAYS_INLINE write_tag_and_fixed32(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                                 uint8_t tag, uint32_t value) {
    PROTO_ENCODE_CHECK_BOUNDS(pos, 5);
    pos[0] = tag;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    std::memcpy(pos + 1, &value, 4);
#else
    pos[1] = static_cast<uint8_t>(value & 0xFF);
    pos[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    pos[3] = static_cast<uint8_t>((value >> 16) & 0xFF);
    pos[4] = static_cast<uint8_t>((value >> 24) & 0xFF);
#endif
    pos += 5;
  }
  static inline void encode_string(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   const char *string, size_t len, bool force = false) {
    if (len == 0 && !force)
      return;
    encode_field_raw(pos PROTO_ENCODE_DEBUG_ARG, field_id, 2);  // type 2: Length-delimited string
    // NOLINTNEXTLINE(readability-inconsistent-ifelse-braces) -- false positive on [[likely]] attribute
    if (len < VARINT_MAX_1_BYTE) [[likely]] {
      PROTO_ENCODE_CHECK_BOUNDS(pos, 1 + len);
      *pos++ = static_cast<uint8_t>(len);
    } else {
      encode_varint_raw_loop(pos PROTO_ENCODE_DEBUG_ARG, len);
      PROTO_ENCODE_CHECK_BOUNDS(pos, len);
    }
    std::memcpy(pos, string, len);
    pos += len;
  }
  static inline void encode_string(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   const std::string &value, bool force = false) {
    encode_string(pos PROTO_ENCODE_DEBUG_ARG, field_id, value.data(), value.size(), force);
  }
  static inline void encode_string(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   const StringRef &ref, bool force = false) {
    encode_string(pos PROTO_ENCODE_DEBUG_ARG, field_id, ref.c_str(), ref.size(), force);
  }
  static inline void encode_bytes(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                  const uint8_t *data, size_t len, bool force = false) {
    encode_string(pos PROTO_ENCODE_DEBUG_ARG, field_id, reinterpret_cast<const char *>(data), len, force);
  }
  static inline void encode_uint32(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    encode_field_raw(pos PROTO_ENCODE_DEBUG_ARG, field_id, 0);
    encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  static inline void encode_uint64(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   uint64_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    encode_field_raw(pos PROTO_ENCODE_DEBUG_ARG, field_id, 0);
    encode_varint_raw_64(pos PROTO_ENCODE_DEBUG_ARG, value);
  }
  static inline void encode_bool(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id, bool value,
                                 bool force = false) {
    if (!value && !force)
      return;
    encode_field_raw(pos PROTO_ENCODE_DEBUG_ARG, field_id, 0);
    PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
    *pos++ = value ? 0x01 : 0x00;
  }
  static inline void encode_fixed32(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                    uint32_t value, bool force = false) {
    if (value == 0 && !force)
      return;
    encode_field_raw(pos PROTO_ENCODE_DEBUG_ARG, field_id, 5);
    PROTO_ENCODE_CHECK_BOUNDS(pos, 4);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    std::memcpy(pos, &value, 4);
    pos += 4;
#else
    *pos++ = (value >> 0) & 0xFF;
    *pos++ = (value >> 8) & 0xFF;
    *pos++ = (value >> 16) & 0xFF;
    *pos++ = (value >> 24) & 0xFF;
#endif
  }
  // NOTE: Wire type 1 (64-bit fixed: double, fixed64, sfixed64) is intentionally
  // not supported to reduce overhead on embedded systems. All ESPHome devices are
  // 32-bit microcontrollers where 64-bit operations are expensive. If 64-bit support
  // is needed in the future, the necessary encoding/decoding functions must be added.
  static inline void encode_float(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id, float value,
                                  bool force = false) {
    uint32_t raw = float_to_raw(value);
    if (raw == 0 && !force)
      return;
    encode_fixed32(pos PROTO_ENCODE_DEBUG_ARG, field_id, raw);
  }
  static inline void encode_int32(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id, int32_t value,
                                  bool force = false) {
    if (value < 0) {
      // negative int32 is always 10 byte long
      encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, field_id, static_cast<uint64_t>(value), force);
      return;
    }
    encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, field_id, static_cast<uint32_t>(value), force);
  }
  static inline void encode_int64(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id, int64_t value,
                                  bool force = false) {
    encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, field_id, static_cast<uint64_t>(value), force);
  }
  static inline void encode_sint32(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   int32_t value, bool force = false) {
    encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, field_id, encode_zigzag32(value), force);
  }
  static inline void encode_sint64(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, uint32_t field_id,
                                   int64_t value, bool force = false) {
    encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, field_id, encode_zigzag64(value), force);
  }
  /// Sub-message encoding: sync pos to buffer, delegate, get pos from return value.
  template<typename T>
  static inline void encode_sub_message(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM, ProtoWriteBuffer &buffer,
                                        uint32_t field_id, const T &value) {
    buffer.set_pos(pos);
    buffer.encode_sub_message(field_id, value);
    pos = buffer.get_pos();
  }
  template<typename T>
  static inline void encode_optional_sub_message(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM,
                                                 ProtoWriteBuffer &buffer, uint32_t field_id, const T &value) {
    buffer.set_pos(pos);
    buffer.encode_optional_sub_message(field_id, value);
    pos = buffer.get_pos();
  }
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

  /// Append a PROGMEM string (flash-safe on ESP8266, regular append on other platforms)
  DumpBuffer &append_p(const char *str) {
    if (str) {
#ifdef USE_ESP8266
      append_p_esp8266(str);
#else
      append_impl_(str, strlen(str));
#endif
    }
    return *this;
  }

#ifdef USE_ESP8266
  /// Out-of-line ESP8266 PROGMEM append to avoid inlining strlen_P/memcpy_P at every call site
  void append_p_esp8266(const char *str);
#endif

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
  uint8_t *encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const { return buffer.get_pos(); }
  uint32_t calculate_size() const { return 0; }
#ifdef HAS_PROTO_MESSAGE_DUMP
  virtual const char *dump_to(DumpBuffer &out) const = 0;
  virtual const LogString *message_name() const { return LOG_STR("unknown"); }
#endif

#ifndef USE_HOST
 protected:
#endif
  // Non-virtual destructor is protected to prevent polymorphic deletion.
  // On host platform, made public to allow value-initialization of std::array
  // members (e.g. DeviceInfoResponse::devices) without clang errors.
  ~ProtoMessage() = default;
};

// Base class for messages that support decoding
class ProtoDecodableMessage : public ProtoMessage {
 public:
  void decode(const uint8_t *buffer, size_t length);

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
  virtual bool decode_varint(uint32_t field_id, proto_varint_value_t value) { return false; }
  virtual bool decode_length(uint32_t field_id, ProtoLengthDelimited value) { return false; }
  virtual bool decode_32bit(uint32_t field_id, Proto32Bit value) { return false; }
  // NOTE: decode_64bit removed - wire type 1 not supported
};

class ProtoSize {
 public:
  // Varint encoding thresholds — use namespace-level constants for 1/2 byte,
  // class-level for 3/4 byte (only used within ProtoSize).
  static constexpr uint32_t VARINT_THRESHOLD_1_BYTE = VARINT_MAX_1_BYTE;
  static constexpr uint32_t VARINT_THRESHOLD_2_BYTE = VARINT_MAX_2_BYTE;
  static constexpr uint32_t VARINT_THRESHOLD_3_BYTE = 1 << 21;  // 2097152
  static constexpr uint32_t VARINT_THRESHOLD_4_BYTE = 1 << 28;  // 268435456

  // Varint encoded length for a 16-bit value (1, 2, or 3 bytes).
  // Fully inline — no slow path call for values >= 128.
  static constexpr inline uint8_t ESPHOME_ALWAYS_INLINE varint16(uint16_t value) {
    return value < VARINT_THRESHOLD_1_BYTE ? 1 : (value < VARINT_THRESHOLD_2_BYTE ? 2 : 3);
  }

  // Varint encoded length for an 8-bit value (1 or 2 bytes).
  static constexpr inline uint8_t ESPHOME_ALWAYS_INLINE varint8(uint8_t value) {
    return value < VARINT_THRESHOLD_1_BYTE ? 1 : 2;
  }

  /**
   * @brief Calculates the size in bytes needed to encode a uint32_t value as a varint
   *
   * @param value The uint32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE varint(uint32_t value) {
    if (value < VARINT_THRESHOLD_1_BYTE) [[likely]]
      return 1;  // Fast path: 7 bits, most common case
    if (__builtin_is_constant_evaluated())
      return varint_wide(value);
    return varint_slow(value);
  }
  /// Size of a varint expected to be 1-2 bytes (e.g. zigzag RSSI, small lengths).
  /// Inlines both checks; falls back to slow path for 3+ bytes.
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE varint_short(uint32_t value) {
    if (value < VARINT_THRESHOLD_1_BYTE) [[likely]]
      return 1;
    if (value < VARINT_THRESHOLD_2_BYTE) [[likely]]
      return 2;
    if (__builtin_is_constant_evaluated())
      return varint_wide(value);
    return varint_slow(value);
  }

 private:
  // Slow path for varint >= 128, outlined to keep fast path small
  static uint32_t varint_slow(uint32_t value) __attribute__((noinline));
  // Shared cascade for values >= 128 (used by both constexpr and noinline paths)
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE varint_wide(uint32_t value) {
    if (value < VARINT_THRESHOLD_2_BYTE)
      return 2;
    if (value < VARINT_THRESHOLD_3_BYTE)
      return 3;
    if (value < VARINT_THRESHOLD_4_BYTE)
      return 4;
    return 5;
  }

 public:
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
  static uint32_t calc_float(uint32_t field_id_size, float value) {
    return float_to_raw(value) != 0 ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_fixed32(uint32_t field_id_size, uint32_t value) {
    return value ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_sfixed32(uint32_t field_id_size, int32_t value) {
    return value ? field_id_size + 4 : 0;
  }
  static constexpr uint32_t calc_sint32(uint32_t field_id_size, int32_t value) {
    return value ? field_id_size + varint_short(encode_zigzag32(value)) : 0;
  }
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE calc_sint32_force(uint32_t field_id_size, int32_t value) {
    return field_id_size + varint_short(encode_zigzag32(value));
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
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE calc_uint64_force(uint32_t field_id_size, uint64_t value) {
    return field_id_size + varint(value);
  }
  /// 48-bit MAC address variant: matches encode_varint_raw_48bit's fast path.
  /// When any of the top 6 of 48 bits is set the encoded varint is 7 bytes;
  /// otherwise fall back to the general size calculation.
  /// Caller must guarantee value fits in 48 bits (encoder asserts in debug).
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE calc_uint64_48bit_force(uint32_t field_id_size,
                                                                                 uint64_t value) {
    return field_id_size + (value >= (1ULL << (MAC_ADDRESS_SIZE * 8 - 6)) ? 7 : varint(value));
  }
  static constexpr uint32_t calc_length(uint32_t field_id_size, size_t len) {
    return len ? field_id_size + varint(static_cast<uint32_t>(len)) + static_cast<uint32_t>(len) : 0;
  }
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE calc_length_force(uint32_t field_id_size, size_t len) {
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
  static constexpr inline uint32_t ESPHOME_ALWAYS_INLINE calc_message_force(uint32_t field_id_size,
                                                                            uint32_t nested_size) {
    return field_id_size + varint(nested_size) + nested_size;
  }
};

// Implementation of methods that depend on ProtoSize being fully defined

// Encode thunk — converts void* back to concrete type for direct encode() call
template<typename T> uint8_t *proto_encode_msg(const void *msg, ProtoWriteBuffer &buf PROTO_ENCODE_DEBUG_PARAM) {
  return static_cast<const T *>(msg)->encode(buf PROTO_ENCODE_DEBUG_ARG);
}

// Thin template wrapper; delegates to non-template core in proto.cpp.
template<typename T> inline void ProtoWriteBuffer::encode_sub_message(uint32_t field_id, const T &value) {
  this->encode_sub_message(field_id, &value, &proto_encode_msg<T>);
}

// Thin template wrapper; delegates to non-template core.
template<typename T> inline void ProtoWriteBuffer::encode_optional_sub_message(uint32_t field_id, const T &value) {
  this->encode_optional_sub_message(field_id, value.calculate_size(), &value, &proto_encode_msg<T>);
}

// Template decode_to_message - preserves concrete type so decode() resolves statically
template<typename T> void ProtoLengthDelimited::decode_to_message(T &msg) const {
  msg.decode(this->value_, this->length_);
}

template<typename T> const char *proto_enum_to_string(T value);

// ProtoService removed — its methods were inlined into APIConnection.
// APIConnection is the concrete server-side implementation; the extra virtual layer was unnecessary.

}  // namespace esphome::api
