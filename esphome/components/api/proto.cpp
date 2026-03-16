#include "proto.h"
#include <cinttypes>
#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.proto";

uint32_t ProtoSize::varint_slow(uint32_t value) { return varint_wide(value); }

void ProtoWriteBuffer::encode_varint_raw_slow_(uint32_t value) {
  do {
    this->debug_check_bounds_(1);
    *this->pos_++ = static_cast<uint8_t>(value | 0x80);
    value >>= 7;
  } while (value > 0x7F);
  this->debug_check_bounds_(1);
  *this->pos_++ = static_cast<uint8_t>(value);
}

ProtoVarIntResult ProtoVarInt::parse_slow(const uint8_t *buffer, uint32_t len) {
  // Multi-byte varint: first byte already checked to have high bit set
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
      return {result32, i + 1};
    }
  }
#ifdef USE_API_VARINT64
  return parse_wide(buffer, len, result32);
#else
  return {0, PROTO_VARINT_PARSE_FAILED};
#endif
}

#ifdef USE_API_VARINT64
ProtoVarIntResult ProtoVarInt::parse_wide(const uint8_t *buffer, uint32_t len, uint32_t result32) {
  uint64_t result64 = result32;
  uint32_t limit = std::min(len, uint32_t(10));
  for (uint32_t i = 4; i < limit; i++) {
    uint8_t val = buffer[i];
    result64 |= uint64_t(val & 0x7F) << (i * 7);
    if ((val & 0x80) == 0) {
      return {result64, i + 1};
    }
  }
  return {0, PROTO_VARINT_PARSE_FAILED};
}
#endif

uint32_t ProtoDecodableMessage::count_repeated_field(const uint8_t *buffer, size_t length, uint32_t target_field_id) {
  uint32_t count = 0;
  const uint8_t *ptr = buffer;
  const uint8_t *end = buffer + length;

  while (ptr < end) {
    // Parse field header (tag) - ptr < end guarantees len >= 1
    auto res = ProtoVarInt::parse_non_empty(ptr, end - ptr);
    if (!res.has_value()) {
      break;  // Invalid data, stop counting
    }

    uint32_t tag = static_cast<uint32_t>(res.value);
    uint32_t field_type = tag & WIRE_TYPE_MASK;
    uint32_t field_id = tag >> 3;
    ptr += res.consumed;

    // Count if this is the target field
    if (field_id == target_field_id) {
      count++;
    }

    // Skip field data based on wire type
    switch (field_type) {
      case WIRE_TYPE_VARINT: {  // VarInt - parse and skip
        res = ProtoVarInt::parse(ptr, end - ptr);
        if (!res.has_value()) {
          return count;  // Invalid data, return what we have
        }
        ptr += res.consumed;
        break;
      }
      case WIRE_TYPE_LENGTH_DELIMITED: {  // Length-delimited - parse length and skip data
        res = ProtoVarInt::parse(ptr, end - ptr);
        if (!res.has_value()) {
          return count;
        }
        uint32_t field_length = static_cast<uint32_t>(res.value);
        ptr += res.consumed;
        if (field_length > static_cast<size_t>(end - ptr)) {
          return count;  // Out of bounds
        }
        ptr += field_length;
        break;
      }
      case WIRE_TYPE_FIXED32: {  // 32-bit - skip 4 bytes
        if (end - ptr < 4) {
          return count;
        }
        ptr += 4;
        break;
      }
      default:
        // Unknown wire type, can't continue
        return count;
    }
  }

  return count;
}

// Single-pass encode for repeated submessage elements (non-template core).
// Writes field tag, reserves 1 byte for length varint, encodes the submessage body,
// then backpatches the actual length. For the common case (body < 128 bytes), this is
// just a single byte write with no memmove — all current repeated submessage types
// (BLE advertisements at ~47B, GATT descriptors at ~24B, service args, etc.) take
// this fast path.
//
// The memmove fallback for body >= 128 bytes exists only for correctness (e.g., a GATT
// characteristic with many descriptors). It is safe because calculate_size() already
// reserved space for the full multi-byte varint — the shift fills that reserved space:
//
//   calculate_size() allocates per element: tag + varint_size(body) + body_size
//
//   After encode, before memmove (1 byte reserved, body written):
//   [tag][__][body ..... body][??]
//         ^                   ^-- unused byte (v2 space from calculate_size)
//         len_pos
//
//   After memmove(body_start+1, body_start, body_size):
//   [tag][__][__][body ..... body]
//         ^       ^-- body shifted forward, fills v2 space exactly
//         len_pos
//
//   After writing 2-byte varint at len_pos:
//   [tag][v1][v2][body ..... body]
//                                ^-- pos_ = element end, within buffer
void ProtoWriteBuffer::encode_sub_message(uint32_t field_id, const void *value,
                                          void (*encode_fn)(const void *, ProtoWriteBuffer &)) {
  this->encode_field_raw(field_id, 2);
  // Reserve 1 byte for length varint (optimistic: submessage < 128 bytes)
  uint8_t *len_pos = this->pos_;
  this->debug_check_bounds_(1);
  this->pos_++;
  uint8_t *body_start = this->pos_;
  encode_fn(value, *this);
  uint32_t body_size = static_cast<uint32_t>(this->pos_ - body_start);
  if (body_size < 128) [[likely]] {
    // Common case: 1-byte varint, just backpatch
    *len_pos = static_cast<uint8_t>(body_size);
    return;
  }
  // Compute extra bytes needed for varint beyond the 1 already reserved
  uint8_t extra = ProtoSize::varint(body_size) - 1;
  // Shift body forward to make room for the extra varint bytes
  this->debug_check_bounds_(extra);
  std::memmove(body_start + extra, body_start, body_size);
  uint8_t *end = this->pos_ + extra;
  // Write the full varint at len_pos
  this->pos_ = len_pos;
  this->encode_varint_raw(body_size);
  this->pos_ = end;
}

// Non-template core for encode_optional_sub_message.
void ProtoWriteBuffer::encode_optional_sub_message(uint32_t field_id, uint32_t nested_size, const void *value,
                                                   void (*encode_fn)(const void *, ProtoWriteBuffer &)) {
  if (nested_size == 0)
    return;
  this->encode_field_raw(field_id, 2);
  this->encode_varint_raw(nested_size);
#ifdef ESPHOME_DEBUG_API
  uint8_t *start = this->pos_;
  encode_fn(value, *this);
  if (static_cast<uint32_t>(this->pos_ - start) != nested_size)
    this->debug_check_encode_size_(field_id, nested_size, this->pos_ - start);
#else
  encode_fn(value, *this);
#endif
}

#ifdef ESPHOME_DEBUG_API
void ProtoWriteBuffer::debug_check_bounds_(size_t bytes, const char *caller) {
  if (this->pos_ + bytes > this->buffer_->data() + this->buffer_->size()) {
    ESP_LOGE(TAG, "ProtoWriteBuffer bounds check failed in %s: bytes=%zu offset=%td buf_size=%zu", caller, bytes,
             this->pos_ - this->buffer_->data(), this->buffer_->size());
    abort();
  }
}
void ProtoWriteBuffer::debug_check_encode_size_(uint32_t field_id, uint32_t expected, ptrdiff_t actual) {
  ESP_LOGE(TAG, "encode_message: size mismatch for field %" PRIu32 ": calculated=%" PRIu32 " actual=%td", field_id,
           expected, actual);
  abort();
}
#endif

void ProtoDecodableMessage::decode(const uint8_t *buffer, size_t length) {
  const uint8_t *ptr = buffer;
  const uint8_t *end = buffer + length;

  while (ptr < end) {
    // Parse field header - ptr < end guarantees len >= 1
    auto res = ProtoVarInt::parse_non_empty(ptr, end - ptr);
    if (!res.has_value()) {
      ESP_LOGV(TAG, "Invalid field start at offset %ld", (long) (ptr - buffer));
      return;
    }

    uint32_t tag = static_cast<uint32_t>(res.value);
    uint32_t field_type = tag & WIRE_TYPE_MASK;
    uint32_t field_id = tag >> 3;
    ptr += res.consumed;

    switch (field_type) {
      case WIRE_TYPE_VARINT: {  // VarInt
        res = ProtoVarInt::parse(ptr, end - ptr);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid VarInt at offset %ld", (long) (ptr - buffer));
          return;
        }
        if (!this->decode_varint(field_id, res.value)) {
          ESP_LOGV(TAG, "Cannot decode VarInt field %" PRIu32 " with value %" PRIu64 "!", field_id,
                   static_cast<uint64_t>(res.value));
        }
        ptr += res.consumed;
        break;
      }
      case WIRE_TYPE_LENGTH_DELIMITED: {  // Length-delimited
        res = ProtoVarInt::parse(ptr, end - ptr);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid Length Delimited at offset %ld", (long) (ptr - buffer));
          return;
        }
        uint32_t field_length = static_cast<uint32_t>(res.value);
        ptr += res.consumed;
        if (field_length > static_cast<size_t>(end - ptr)) {
          ESP_LOGV(TAG, "Out-of-bounds Length Delimited at offset %ld", (long) (ptr - buffer));
          return;
        }
        if (!this->decode_length(field_id, ProtoLengthDelimited(ptr, field_length))) {
          ESP_LOGV(TAG, "Cannot decode Length Delimited field %" PRIu32 "!", field_id);
        }
        ptr += field_length;
        break;
      }
      case WIRE_TYPE_FIXED32: {  // 32-bit
        if (end - ptr < 4) {
          ESP_LOGV(TAG, "Out-of-bounds Fixed32-bit at offset %ld", (long) (ptr - buffer));
          return;
        }
        uint32_t val = encode_uint32(ptr[3], ptr[2], ptr[1], ptr[0]);
        if (!this->decode_32bit(field_id, Proto32Bit(val))) {
          ESP_LOGV(TAG, "Cannot decode 32-bit field %" PRIu32 " with value %" PRIu32 "!", field_id, val);
        }
        ptr += 4;
        break;
      }
      default:
        ESP_LOGV(TAG, "Invalid field type %" PRIu32 " at offset %ld", field_type, (long) (ptr - buffer));
        return;
    }
  }
}

}  // namespace esphome::api
