#pragma once

#include <cctype>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

#ifdef USE_API_HOMEASSISTANT_SERVICES
#include "esphome/components/api/api_pb2.h"
#endif

namespace esphome::homeassistant {

/// True for what Home Assistant reports when there is no value: "None" for an attribute that is not
/// set, "unavailable" and "unknown" for the state of an entity that is not ready
bool is_unset(StringRef value);

/// True when entity_id belongs to an input helper. input_service is the helper's action name in flash, such as
/// "input_number.set_value"; it must start with "input_", as only those first 6 characters are compared
inline bool is_input_helper(const char *entity_id, const char *input_service) {
  return ESPHOME_strncmp_P(entity_id, input_service, sizeof("input_") - 1) == 0;
}

#ifdef USE_API_HOMEASSISTANT_SERVICES
/// Adds the entity_id entry to an action's data. Inline, as the control platforms using it do not all
/// compile homeassistant.cpp
inline void add_entity_id(api::HomeassistantActionRequest &resp, const char *entity_id) {
  // In flash on the ESP8266 and encoded from there
  static constexpr char ENTITY_ID_KEY[] PROGMEM = "entity_id";
  auto &entry = resp.data.emplace_back();
  entry.key = ProgmemStringRef::from_lit(ENTITY_ID_KEY);
  entry.value = ProgmemStringRef(StringRef(entity_id));
}
#endif

/// Emit the UTF-8 encoding of a code point. Rejects NUL, which would end the item early.
template<typename C> bool emit_utf8(uint32_t code_point, C &on_char) {
  if (code_point == 0 || code_point > 0x10FFFF)
    return false;
  if (code_point < 0x80) {
    on_char(static_cast<char>(code_point));
  } else if (code_point < 0x800) {
    on_char(static_cast<char>(0xC0 | (code_point >> 6)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point < 0x10000) {
    on_char(static_cast<char>(0xE0 | (code_point >> 12)));
    on_char(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else {
    on_char(static_cast<char>(0xF0 | (code_point >> 18)));
    on_char(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    on_char(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  return true;
}

/** Walk a list attribute as Home Assistant sends it: the Python repr of a list of strings, such as
 * `['Low', "It's high", 'caf\xe9']`, or of an enum, such as `[<ColorMode.HS: 'hs'>]`, where the
 * quoted value stands for the item.
 *
 * Calls on_char for each decoded byte of an item and on_item at the end of each item. Returns false
 * on malformed input; the callbacks may already have run by then.
 */
template<typename C, typename O> bool parse_python_list(StringRef src, C &&on_char, O &&on_item) {
  const char *pos = src.c_str();
  const char *const end = pos + src.size();
  auto skip_spaces = [&]() {
    while (pos < end && *pos == ' ')
      pos++;
  };

  skip_spaces();
  if (pos == end || *pos != '[')
    return false;
  pos++;
  skip_spaces();
  if (pos < end && *pos == ']') {
    pos++;
  } else {
    while (true) {
      skip_spaces();
      // An enum member is shown as <Name.MEMBER: 'value'>; the quoted value is the item
      const bool enum_member = pos < end && *pos == '<';
      if (enum_member) {
        pos++;
        while (pos < end && (std::isalnum(static_cast<unsigned char>(*pos)) || *pos == '_' || *pos == '.'))
          pos++;
        if (pos == end || *pos != ':')
          return false;
        pos++;
        skip_spaces();
      }
      if (pos == end || (*pos != '\'' && *pos != '"'))
        return false;
      const char quote = *pos++;
      while (true) {
        if (pos == end)
          return false;
        const char c = *pos++;
        if (c == quote)
          break;
        if (c != '\\') {
          on_char(c);
          continue;
        }
        if (pos == end)
          return false;
        const char escape = *pos++;
        switch (escape) {
          case '\\':
          case '\'':
          case '"':
            on_char(escape);
            break;
          case 'n':
            on_char('\n');
            break;
          case 'r':
            on_char('\r');
            break;
          case 't':
            on_char('\t');
            break;
          case 'x':
          case 'u':
          case 'U': {
            const size_t digits = escape == 'x' ? 2 : (escape == 'u' ? 4 : 8);
            if (static_cast<size_t>(end - pos) < digits)
              return false;
            auto code_point = parse_hex<uint32_t>(pos, digits);
            if (!code_point.has_value() || !emit_utf8(*code_point, on_char))
              return false;
            pos += digits;
            break;
          }
          default:
            return false;
        }
      }
      if (enum_member) {
        skip_spaces();
        if (pos == end || *pos != '>')
          return false;
        pos++;
      }
      on_item();
      skip_spaces();
      if (pos == end)
        return false;
      const char separator = *pos++;
      if (separator == ']')
        break;
      if (separator != ',')
        return false;
    }
  }
  skip_spaces();
  return pos == end;
}

}  // namespace esphome::homeassistant
