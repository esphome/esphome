#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

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

  uint32_t as_uint32() const { return this->value_; }
  uint64_t as_uint64() const { return this->value_; }
  bool as_bool() const { return this->value_; }
  template<typename T> T as_enum() const { return static_cast<T>(this->as_uint32()); }
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

class ProtoLengthDelimited {
 public:
  explicit ProtoLengthDelimited(const uint8_t *value, size_t length) : value_(value), length_(length) {}
  std::string as_string() const { return std::string(reinterpret_cast<const char *>(this->value_), this->length_); }
  template<class C> C as_message() const {
    auto msg = C();
    msg.decode(this->value_, this->length_);
    return msg;
  }

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
    this->encode_string(field_id, value.data(), value.size());
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
  template<typename T> void encode_enum(uint32_t field_id, T value, bool force = false) {
    this->encode_uint32(field_id, static_cast<uint32_t>(value), force);
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
  template<class C> void encode_message(uint32_t field_id, const C &value, bool force = false) {
    this->encode_field_raw(field_id, 2);  // type 2: Length-delimited message
    size_t begin = this->buffer_->size();

    value.encode(*this);

    const uint32_t nested_length = this->buffer_->size() - begin;
    // add size varint
    std::vector<uint8_t> var;
    ProtoVarInt(nested_length).encode(var);
    this->buffer_->insert(this->buffer_->begin() + begin, var.begin(), var.end());
  }
  std::vector<uint8_t> *get_buffer() const { return buffer_; }

 protected:
  std::vector<uint8_t> *buffer_;
};

class ProtoMessage {
 public:
  virtual ~ProtoMessage() = default;
  virtual void encode(ProtoWriteBuffer buffer) const = 0;
  void decode(const uint8_t *buffer, size_t length);
  virtual void calculate_size(uint32_t &total_size) const = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  std::string dump() const;
  virtual void dump_to(std::string &out) const = 0;
#endif

 protected:
  virtual bool decode_varint(uint32_t field_id, ProtoVarInt value) { return false; }
  virtual bool decode_length(uint32_t field_id, ProtoLengthDelimited value) { return false; }
  virtual bool decode_32bit(uint32_t field_id, Proto32Bit value) { return false; }
  virtual bool decode_64bit(uint32_t field_id, Proto64Bit value) { return false; }
};

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
  virtual bool send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) = 0;
  virtual bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) = 0;

  // Optimized method that pre-allocates buffer based on message size
  template<class C> bool send_message_(const C &msg, uint32_t message_type) {
    uint32_t msg_size = 0;
    msg.calculate_size(msg_size);

    // Create a pre-sized buffer
    auto buffer = this->create_buffer(msg_size);

    // Encode message into the buffer
    msg.encode(buffer);

    // Send the buffer
    return this->send_buffer(buffer, message_type);
  }
};

}  // namespace api
}  // namespace esphome
