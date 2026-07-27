#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::captive_portal {

/// Largest number of output bytes a single input byte can expand to (a \u00XX sequence).
static constexpr size_t JSON_ESCAPE_MAX_EXPANSION = 6;

/// Copy value into buf, escaping the characters that cannot appear raw inside a JSON string literal.
///
/// Escapes " and \ along with the control characters below 0x20, using the short forms where JSON defines one and
/// \u00XX otherwise. Bytes >= 0x20 are copied verbatim, so text containing valid UTF-8 survives intact. The result is
/// always null terminated; anything that would not fit is dropped rather than written partially. Returns buf so the
/// call can be used directly as an argument.
///
/// To size buf so that no input is ever dropped, allow JSON_ESCAPE_MAX_EXPANSION bytes per input byte plus one for
/// the null terminator.
inline const char *json_escape_into_buffer(std::span<char> buf, StringRef value) {
  if (buf.empty())
    return "";
  // Reserve one byte for the null terminator.
  const size_t limit = buf.size() - 1;
  size_t pos = 0;
  for (char ch : value) {
    auto c = static_cast<unsigned char>(ch);
    // Every short form is a backslash followed by a single character, so only that character is needed here. Keeping
    // it a char rather than a string avoids putting the sequences in read only data, which is RAM on the ESP8266.
    char escape = '\0';
    switch (c) {
      case '"':
        escape = '"';
        break;
      case '\\':
        escape = '\\';
        break;
      case '\n':
        escape = 'n';
        break;
      case '\r':
        escape = 'r';
        break;
      case '\t':
        escape = 't';
        break;
      case '\b':
        escape = 'b';
        break;
      case '\f':
        escape = 'f';
        break;
      default:
        break;
    }
    if (escape != '\0') {
      if (pos + 2 > limit)
        break;
      buf[pos++] = '\\';
      buf[pos++] = escape;
    } else if (c < 0x20) {
      // Remaining control characters have no short form and must be written as \u00XX. The value is below 0x20, so
      // the two high hex digits are always zero.
      if (pos + JSON_ESCAPE_MAX_EXPANSION > limit)
        break;
      buf[pos++] = '\\';
      buf[pos++] = 'u';
      buf[pos++] = '0';
      buf[pos++] = '0';
      buf[pos++] = format_hex_char(static_cast<uint8_t>(c >> 4));
      buf[pos++] = format_hex_char(static_cast<uint8_t>(c & 0x0F));
    } else {
      if (pos + 1 > limit)
        break;
      buf[pos++] = static_cast<char>(c);
    }
  }
  buf[pos] = '\0';
  return buf.data();
}

}  // namespace esphome::captive_portal
