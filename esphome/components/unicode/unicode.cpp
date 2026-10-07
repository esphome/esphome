#include "unicode.h"

namespace esphome::unicode {

uint32_t extract_unicode_codepoint(const char *utf8_str, size_t *length) {
  // Safely cast to uint8_t* for correct bitwise operations on bytes
  const uint8_t *current = reinterpret_cast<const uint8_t *>(utf8_str);
  uint32_t code_point = 0;
  uint8_t c1 = *current++;

  // check for end of string
  if (c1 == 0) {
    *length = 0;
    return 0;
  }

  // --- 1-Byte Sequence: 0xxxxxxx (ASCII) ---
  if (c1 < 0x80) {
    // Valid ASCII byte.
    code_point = c1;
    // Optimization: No need to check for continuation bytes.
  }
  // --- 2-Byte Sequence: 110xxxxx 10xxxxxx ---
  else if ((c1 & 0xE0) == 0xC0) {
    uint8_t c2 = *current++;

    // Error Check 1: Check if c2 is a valid continuation byte (10xxxxxx)
    if ((c2 & 0xC0) != 0x80) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x1F) << 6;
    code_point |= (c2 & 0x3F);

    // Error Check 2: Overlong check (2-byte must be > 0x7F)
    if (code_point <= 0x7F) {
      *length = 0;
      return 0;
    }
  }
  // --- 3-Byte Sequence: 1110xxxx 10xxxxxx 10xxxxxx ---
  else if ((c1 & 0xF0) == 0xE0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;

    // Error Check 1: Check continuation bytes
    if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x0F) << 12;
    code_point |= (c2 & 0x3F) << 6;
    code_point |= (c3 & 0x3F);

    // Error Check 2: Overlong check (3-byte must be > 0x7FF)
    // Also check for surrogates (0xD800-0xDFFF)
    if (code_point <= 0x7FF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
      *length = 0;
      return 0;
    }
  }
  // --- 4-Byte Sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx ---
  else if ((c1 & 0xF8) == 0xF0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;
    uint8_t c4 = *current++;

    // Error Check 1: Check continuation bytes
    if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80) || ((c4 & 0xC0) != 0x80)) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x07) << 18;
    code_point |= (c2 & 0x3F) << 12;
    code_point |= (c3 & 0x3F) << 6;
    code_point |= (c4 & 0x3F);

    // Error Check 2: Overlong check (4-byte must be > 0xFFFF)
    // Also check for valid Unicode range (must be <= 0x10FFFF)
    if (code_point <= 0xFFFF || code_point > 0x10FFFF) {
      *length = 0;
      return 0;
    }
  }
  // --- Invalid leading byte (e.g., 10xxxxxx or 11111xxx) ---
  else {
    *length = 0;
    return 0;
  }
  *length = current - reinterpret_cast<const uint8_t *>(utf8_str);
  return code_point;
}

}  // namespace esphome::unicode
