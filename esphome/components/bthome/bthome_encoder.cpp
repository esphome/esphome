#include "bthome_encoder.h"

#include <cmath>
#include <cstring>

namespace esphome {
namespace bthome {
namespace server {

void BTHomeEncoder::reset() { this->offset_ = 0; }

bool BTHomeEncoder::write_raw_(BTHomeObjectType type, const uint8_t *data, size_t length) {
  size_t needed = 1 + length;  // 1 byte for type + value bytes
  if (this->offset_ + needed > BTHOME_SERVER_MAX_PAYLOAD) {
    return false;
  }
  this->buffer_[this->offset_++] = static_cast<uint8_t>(type);
  memcpy(&this->buffer_[this->offset_], data, length);
  this->offset_ += length;
  return true;
}

bool BTHomeEncoder::write_float(BTHomeObjectType type, float value) {
  size_t length = get_bthome_value_length(type);
  if (length == 0) {
    return false;  // Variable-length or unknown type
  }

  float scale = bthome_scaling_factor(type);
  bool is_signed = bthome_is_signed(type);

  // Convert float to raw integer: reverse of decoder's as_float()
  // Decoder does: float = scaling_factor * raw_int
  // Encoder does: raw_int = round(float / scaling_factor)
  float raw_f = std::round(value / scale);

  uint8_t data[4];
  if (is_signed) {
    int32_t raw = static_cast<int32_t>(raw_f);
    // Clamp to valid range for the byte width
    switch (length) {
      case 1:
        if (raw < -128)
          raw = -128;
        if (raw > 127)
          raw = 127;
        break;
      case 2:
        if (raw < -32768)
          raw = -32768;
        if (raw > 32767)
          raw = 32767;
        break;
      // 3-byte signed not used in BTHome, but handle defensively
      case 3:
        if (raw < -8388608)
          raw = -8388608;
        if (raw > 8388607)
          raw = 8388607;
        break;
      default:
        break;  // 4-byte: int32_t range is the full range
    }
    // Write as little-endian
    uint32_t u = static_cast<uint32_t>(raw);
    for (size_t i = 0; i < length; i++) {
      data[i] = static_cast<uint8_t>(u & 0xFF);
      u >>= 8;
    }
  } else {
    uint32_t raw = static_cast<uint32_t>(raw_f);
    // Clamp to valid range for the byte width
    switch (length) {
      case 1:
        if (raw > 255)
          raw = 255;
        break;
      case 2:
        if (raw > 65535)
          raw = 65535;
        break;
      case 3:
        if (raw > 16777215)
          raw = 16777215;
        break;
      default:
        break;  // 4-byte: uint32_t range is the full range
    }
    // Write as little-endian
    for (size_t i = 0; i < length; i++) {
      data[i] = static_cast<uint8_t>(raw & 0xFF);
      raw >>= 8;
    }
  }

  return this->write_raw_(type, data, length);
}

bool BTHomeEncoder::write_bool(BTHomeObjectType type, bool value) {
  uint8_t data = value ? 0x01 : 0x00;
  return this->write_raw_(type, &data, 1);
}

}  // namespace server
}  // namespace bthome
}  // namespace esphome
