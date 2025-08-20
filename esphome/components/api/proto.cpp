#include "proto.h"
#include <cinttypes>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.proto";

void ProtoDecodableMessage::decode(const uint8_t *buffer, size_t length) {
  const uint8_t *ptr = buffer;
  const uint8_t *end = buffer + length;

  while (ptr < end) {
    uint32_t consumed;

    // Parse field header
    auto res = ProtoVarInt::parse(ptr, end - ptr, &consumed);
    if (!res.has_value()) {
      ESP_LOGV(TAG, "Invalid field start at offset %ld", (long) (ptr - buffer));
      return;
    }

    uint32_t tag = res->as_uint32();
    uint32_t field_type = tag & 0b111;
    uint32_t field_id = tag >> 3;
    ptr += consumed;

    switch (field_type) {
      case 0: {  // VarInt
        res = ProtoVarInt::parse(ptr, end - ptr, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid VarInt at offset %ld", (long) (ptr - buffer));
          return;
        }
        if (!this->decode_varint(field_id, *res)) {
          ESP_LOGV(TAG, "Cannot decode VarInt field %" PRIu32 " with value %" PRIu32 "!", field_id, res->as_uint32());
        }
        ptr += consumed;
        break;
      }
      case 2: {  // Length-delimited
        res = ProtoVarInt::parse(ptr, end - ptr, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid Length Delimited at offset %ld", (long) (ptr - buffer));
          return;
        }
        uint32_t field_length = res->as_uint32();
        ptr += consumed;
        if (ptr + field_length > end) {
          ESP_LOGV(TAG, "Out-of-bounds Length Delimited at offset %ld", (long) (ptr - buffer));
          return;
        }
        if (!this->decode_length(field_id, ProtoLengthDelimited(ptr, field_length))) {
          ESP_LOGV(TAG, "Cannot decode Length Delimited field %" PRIu32 "!", field_id);
        }
        ptr += field_length;
        break;
      }
      case 5: {  // 32-bit
        if (ptr + 4 > end) {
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
        ESP_LOGV(TAG, "Invalid field type %u at offset %ld", field_type, (long) (ptr - buffer));
        return;
    }
  }
}

#ifdef HAS_PROTO_MESSAGE_DUMP
std::string ProtoMessage::dump() const {
  std::string out;
  this->dump_to(out);
  return out;
}
#endif

}  // namespace esphome::api
