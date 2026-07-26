#pragma once
#include <cstdio>
#include <string>

namespace esphome::captive_portal {

// Escape a string so it can be safely embedded inside a JSON string literal. A WiFi SSID can contain any bytes,
// including a " or \ (or a control character) that would otherwise produce invalid JSON. Bytes >= 0x20 are passed
// through verbatim so valid UTF-8 SSIDs survive intact.
inline std::string json_escape(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += ch;
        }
    }
  }
  return out;
}

}  // namespace esphome::captive_portal
