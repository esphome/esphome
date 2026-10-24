#include "unicode.h"

namespace esphome::unicode {

// Reads count continuation bytes into code_point, stopping at the first byte that is not one so a
// sequence cut short by the terminator is never read past
static bool read_continuation(const uint8_t *&current, uint8_t count, uint32_t &code_point) {
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t c = *current;
    if ((c & 0xC0) != 0x80) {
      return false;
    }
    current++;
    code_point = (code_point << 6) | (c & 0x3F);
  }
  return true;
}

uint32_t extract_unicode_codepoint(const char *utf8_str, size_t *length) {
  const auto *start = reinterpret_cast<const uint8_t *>(utf8_str);
  const uint8_t *current = start;
  const uint8_t c1 = *current++;
  uint8_t count;
  uint32_t code_point;
  uint32_t lowest;  // the smallest code point this sequence length may encode; shorter forms are overlong
  if (c1 == 0) {
    *length = 0;
    return 0;
  }
  if (c1 < 0x80) {
    count = 0;
    code_point = c1;
    lowest = 0;
  } else if ((c1 & 0xE0) == 0xC0) {
    count = 1;
    code_point = c1 & 0x1F;
    lowest = 0x80;
  } else if ((c1 & 0xF0) == 0xE0) {
    count = 2;
    code_point = c1 & 0x0F;
    lowest = 0x800;
  } else if ((c1 & 0xF8) == 0xF0) {
    count = 3;
    code_point = c1 & 0x07;
    lowest = 0x10000;
  } else {
    *length = 0;  // a continuation byte or an invalid lead byte
    return 0;
  }
  if (!read_continuation(current, count, code_point) || code_point < lowest || code_point > 0x10FFFF ||
      (code_point >= 0xD800 && code_point <= 0xDFFF)) {
    *length = 0;
    return 0;
  }
  *length = current - start;
  return code_point;
}

}  // namespace esphome::unicode
