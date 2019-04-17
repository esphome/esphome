#include "api_message.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *TAG = "api.message";

bool APIMessage::decode_varint(uint32_t field_id, uint32_t value) { return false; }
bool APIMessage::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) { return false; }
bool APIMessage::decode_32bit(uint32_t field_id, uint32_t value) { return false; }
void APIMessage::encode(APIBuffer &buffer) {}
void APIMessage::decode(const uint8_t *buffer, size_t length) {
  uint32_t i = 0;
  bool error = false;
  while (i < length) {
    uint32_t consumed;
    auto res = proto_decode_varuint32(&buffer[i], length - i, &consumed);
    if (!res.has_value()) {
      ESP_LOGV(TAG, "Invalid field start at %u", i);
      break;
    }

    uint32_t field_type = (*res) & 0b111;
    uint32_t field_id = (*res) >> 3;
    i += consumed;

    switch (field_type) {
      case 0: {  // VarInt
        res = proto_decode_varuint32(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid VarInt at %u", i);
          error = true;
          break;
        }
        if (!this->decode_varint(field_id, *res)) {
          ESP_LOGV(TAG, "Cannot decode VarInt field %u with value %u!", field_id, *res);
        }
        i += consumed;
        break;
      }
      case 2: {  // Length-delimited
        res = proto_decode_varuint32(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid Length Delimited at %u", i);
          error = true;
          break;
        }
        i += consumed;
        if (*res > length - i) {
          ESP_LOGV(TAG, "Out-of-bounds Length Delimited at %u", i);
          error = true;
          break;
        }
        if (!this->decode_length_delimited(field_id, &buffer[i], *res)) {
          ESP_LOGV(TAG, "Cannot decode Length Delimited field %u!", field_id);
        }
        i += *res;
        break;
      }
      case 5: {  // 32-bit
        if (length - i < 4) {
          ESP_LOGV(TAG, "Out-of-bounds Fixed32-bit at %u", i);
          error = true;
          break;
        }
        uint32_t val = (uint32_t(buffer[i]) << 0) | (uint32_t(buffer[i + 1]) << 8) | (uint32_t(buffer[i + 2]) << 16) |
                       (uint32_t(buffer[i + 3]) << 24);
        if (!this->decode_32bit(field_id, val)) {
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

}  // namespace api
}  // namespace esphome
