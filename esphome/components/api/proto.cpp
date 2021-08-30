#include "proto.h"
#include "util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.proto";

void ProtoMessage::decode(const uint8_t *buffer, size_t length) {
  uint32_t i = 0;
  bool error = false;
  while (i < length) {
    uint32_t consumed;
    auto res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
    if (!res.has_value()) {
      ESP_LOGV(TAG, "Invalid field start at %u", i);
      break;
    }

    uint32_t field_type = (res->as_uint32()) & 0b111;
    uint32_t field_id = (res->as_uint32()) >> 3;
    i += consumed;

    switch (field_type) {
      case 0: {  // VarInt
        res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid VarInt at %u", i);
          error = true;
          break;
        }
        if (!this->decode_varint(field_id, *res)) {
          ESP_LOGV(TAG, "Cannot decode VarInt field %u with value %u!", field_id, res->as_uint32());
        }
        i += consumed;
        break;
      }
      case 2: {  // Length-delimited
        res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid Length Delimited at %u", i);
          error = true;
          break;
        }
        uint32_t field_length = res->as_uint32();
        i += consumed;
        if (field_length > length - i) {
          ESP_LOGV(TAG, "Out-of-bounds Length Delimited at %u", i);
          error = true;
          break;
        }
        if (!this->decode_length(field_id, ProtoLengthDelimited(&buffer[i], field_length))) {
          ESP_LOGV(TAG, "Cannot decode Length Delimited field %u!", field_id);
        }
        i += field_length;
        break;
      }
      case 5: {  // 32-bit
        if (length - i < 4) {
          ESP_LOGV(TAG, "Out-of-bounds Fixed32-bit at %u", i);
          error = true;
          break;
        }
        uint32_t val = encode_uint32(buffer[i + 3], buffer[i + 2], buffer[i + 1], buffer[i]);
        if (!this->decode_32bit(field_id, Proto32Bit(val))) {
          ESP_LOGV(TAG, "Cannot decode 32-bit field %u with value %u!", field_id, val);
        }
        i += 4;
        break;
      }
      default:
        ESP_LOGV(TAG, "Invalid field type at %u", i);
        error = true;
        break;
    }
    if (error) {
      break;
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

}  // namespace api
}  // namespace esphome
