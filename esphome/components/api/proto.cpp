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
// Reserves 1 byte for length varint, encodes the submessage body,
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
//                                ^-- returned cursor = element end, within buffer
uint8_t *ProtoEncode::encode_sub_message_body(uint8_t *__restrict__ pos PROTO_ENCODE_DEBUG_PARAM, const void *value,
                                              ProtoEncodeFn encode_fn) {
  // Reserve 1 byte for the length varint (optimistic: submessage < 128 bytes)
  uint8_t *len_pos = pos;
  PROTO_ENCODE_CHECK_BOUNDS(pos, 1);
  uint8_t *body_start = pos + 1;
  uint8_t *after_body = encode_fn(value, body_start PROTO_ENCODE_DEBUG_ARG);
  uint32_t body_size = static_cast<uint32_t>(after_body - body_start);
  if (body_size < VARINT_MAX_1_BYTE) [[likely]] {
    // Common case: 1-byte varint, just backpatch
    *len_pos = static_cast<uint8_t>(body_size);
    return after_body;
  }
  // Shift the body forward to make room for the extra length varint bytes
  uint8_t extra = ProtoSize::varint(body_size) - 1;
  PROTO_ENCODE_CHECK_BOUNDS(after_body, extra);
  std::memmove(body_start + extra, body_start, body_size);
  // Write the full varint at len_pos
  (void) encode_varint_raw_loop(len_pos PROTO_ENCODE_DEBUG_ARG, body_size);
  return after_body + extra;
}

// Non-template core for encode_optional_sub_message.
uint8_t *ProtoEncode::encode_sized_sub_message_body(uint8_t *__restrict__ pos PROTO_ENCODE_DEBUG_PARAM,
                                                    uint32_t nested_size, const void *value, ProtoEncodeFn encode_fn) {
  pos = encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, nested_size);
  return encode_fn(value, pos PROTO_ENCODE_DEBUG_ARG);
}

#ifdef ESPHOME_DEBUG_API
void proto_check_bounds_failed(const uint8_t *pos, size_t bytes, const uint8_t *end, const char *caller) {
  ESP_LOGE(TAG, "Proto encode bounds check failed in %s: need %zu bytes, %td available", caller, bytes, end - pos);
  abort();
}
void proto_check_encode_end(const uint8_t *end, const uint8_t *expected) {
  if (end == expected)
    return;
  ESP_LOGE(TAG, "Proto encode ended %td bytes off the calculated size", end - expected);
  abort();
}
void proto_check_sub_message_size(uint32_t field_id, uint32_t expected, const uint8_t *len_pos, const uint8_t *end) {
  ptrdiff_t actual = end - (len_pos + ProtoSize::varint(expected));
  if (actual == static_cast<ptrdiff_t>(expected))
    return;
  ESP_LOGE(TAG, "encode_message: size mismatch for field %" PRIu32 ": calculated=%" PRIu32 " actual=%td", field_id,
           expected, actual);
  abort();
}
void ProtoWriteBuffer::debug_check_bounds_(size_t bytes, const char *caller) {
  if (this->pos_ + bytes > this->buffer_->data() + this->buffer_->size()) {
    ESP_LOGE(TAG, "ProtoWriteBuffer bounds check failed in %s: bytes=%zu offset=%td buf_size=%zu", caller, bytes,
             this->pos_ - this->buffer_->data(), this->buffer_->size());
    abort();
  }
}

#endif

void ProtoDecodableMessage::decode_fields(void *msg, const uint8_t *buffer, size_t length, DecodeFieldFn field) {
  const uint8_t *ptr = buffer;
  const uint8_t *end = buffer + length;

  // Single-byte varints dominate, so that case advances the cursor inline.
  auto read_varint = [&](proto_varint_value_t &value) ESPHOME_ALWAYS_INLINE {
    if (ptr == end)
      return false;
    if (*ptr < 0x80) [[likely]] {
      value = *ptr++;
      return true;
    }
    auto res = ProtoVarInt::parse_non_empty(ptr, end - ptr);
    if (!res.has_value())
      return false;
    value = res.value;
    ptr += res.consumed;
    return true;
  };

  while (ptr < end) {
    proto_varint_value_t tag_value;
    if (!read_varint(tag_value)) {
      ESP_LOGV(TAG, "Invalid field start at offset %ld", (long) (ptr - buffer));
      return;
    }

    uint32_t tag = static_cast<uint32_t>(tag_value);
    uint32_t field_type = tag & WIRE_TYPE_MASK;
    // Length-delimited fields move this past the length prefix
    const uint8_t *data = ptr;
    proto_varint_value_t scalar;

    if (field_type == WIRE_TYPE_VARINT) [[likely]] {
      if (!read_varint(scalar)) {
        ESP_LOGV(TAG, "Invalid VarInt at offset %ld", (long) (ptr - buffer));
        return;
      }
    } else {
      switch (field_type) {
        case WIRE_TYPE_LENGTH_DELIMITED: {
          proto_varint_value_t length_value;
          if (!read_varint(length_value)) {
            ESP_LOGV(TAG, "Invalid Length Delimited at offset %ld", (long) (ptr - buffer));
            return;
          }
          uint32_t field_length = static_cast<uint32_t>(length_value);
          if (field_length > static_cast<size_t>(end - ptr)) {
            ESP_LOGV(TAG, "Out-of-bounds Length Delimited at offset %ld", (long) (ptr - buffer));
            return;
          }
          data = ptr;
          scalar = field_length;
          ptr += field_length;
          break;
        }
        case WIRE_TYPE_FIXED32: {
          if (end - ptr < 4) {
            ESP_LOGV(TAG, "Out-of-bounds Fixed32-bit at offset %ld", (long) (ptr - buffer));
            return;
          }
          // Byte loads instead of memcpy: ESP-IDF passes -fno-builtin-memcpy, which made this a call
          scalar = encode_uint32(ptr[3], ptr[2], ptr[1], ptr[0]);
          ptr += 4;
          break;
        }
        default:
          ESP_LOGV(TAG, "Invalid field type %" PRIu32 " at offset %ld", field_type, (long) (ptr - buffer));
          return;
      }
    }
    field(msg, tag, data, scalar);
  }
}

}  // namespace esphome::api
