#include "text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::bthome::client {

ESPHOME_LOG_TAG(TAG, "bthome.text_sensor");

// A RAW value is at most 255 bytes (its length is one byte), published as two hex digits per byte.
static constexpr size_t MAX_VARIABLE_LENGTH = UINT8_MAX;
static constexpr size_t HEX_DIGITS_PER_BYTE = 2;

namespace {

// Lead byte patterns of multi-byte UTF-8 sequences.
struct Utf8Lead {
  uint8_t mask;
  uint8_t value;
  uint8_t continuation_bytes;
  uint32_t min_code_point;  // smaller values are overlong encodings
};
constexpr Utf8Lead UTF8_LEADS[] = {
    {0xE0, 0xC0, 1, 0x80},
    {0xF0, 0xE0, 2, 0x800},
    {0xF8, 0xF0, 3, 0x10000},
};
constexpr uint8_t UTF8_ASCII_LIMIT = 0x80;
constexpr uint8_t UTF8_CONTINUATION_MASK = 0xC0;
constexpr uint8_t UTF8_CONTINUATION_VALUE = 0x80;
constexpr uint8_t UTF8_CONTINUATION_BITS = 6;
constexpr uint32_t UNICODE_MAX = 0x10FFFF;
constexpr uint32_t SURROGATE_FIRST = 0xD800;
constexpr uint32_t SURROGATE_LAST = 0xDFFF;

}  // namespace

// Strict UTF-8 check (no overlong forms, surrogates or values above U+10FFFF), the same rules the
// protobuf decoder applies to string fields on the Home Assistant side.
static bool is_valid_utf8(const uint8_t *data, size_t length) {
  size_t i = 0;
  while (i < length) {
    const uint8_t lead_byte = data[i];
    if (lead_byte < UTF8_ASCII_LIMIT) {
      i++;
      continue;
    }
    const Utf8Lead *lead = nullptr;
    for (const Utf8Lead &candidate : UTF8_LEADS) {
      if ((lead_byte & candidate.mask) == candidate.value) {
        lead = &candidate;
        break;
      }
    }
    if (lead == nullptr || length - i <= lead->continuation_bytes)
      return false;
    uint32_t code_point = lead_byte & static_cast<uint8_t>(~lead->mask);
    for (size_t k = 1; k <= lead->continuation_bytes; k++) {
      const uint8_t byte = data[i + k];
      if ((byte & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_VALUE)
        return false;
      code_point = (code_point << UTF8_CONTINUATION_BITS) | (byte & static_cast<uint8_t>(~UTF8_CONTINUATION_MASK));
    }
    if (code_point < lead->min_code_point || code_point > UNICODE_MAX ||
        (code_point >= SURROGATE_FIRST && code_point <= SURROGATE_LAST))
      return false;
    i += lead->continuation_bytes + 1;
  }
  return true;
}

bool BTHomeTextSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;

  if (object.type == BTHomeObjectType::RAW) {
    // Text sensor states must be valid UTF-8, so RAW bytes are published as lowercase hex.
    this->raw_data_callback_.call(std::span<const uint8_t>(object.data, object.length));
    char hex[MAX_VARIABLE_LENGTH * HEX_DIGITS_PER_BYTE + 1];
    format_hex_to(hex, object.data, object.length);
    this->publish_state(hex, object.length * HEX_DIGITS_PER_BYTE);
    return true;
  }

  if (!is_valid_utf8(object.data, object.length)) {
    ESP_LOGW(TAG, "'%s': ignoring BTHome text that is not valid UTF-8", this->get_name().c_str());
    return true;
  }
  // An empty value may come without a data pointer; publish_state must not get a null pointer.
  const char *text = object.length == 0 ? "" : reinterpret_cast<const char *>(object.data);
  this->publish_state(text, object.length);
  return true;
}

void BTHomeTextSensor::log_config() {
  LOG_TEXT_SENSOR("    ", "Text Sensor", this);
  ESP_LOGCONFIG(TAG, "      Object type: 0x%02X", static_cast<unsigned>(this->object_type_));
}

}  // namespace esphome::bthome::client
