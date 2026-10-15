#include "decoders.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace esphome::fendt_caravan {

bool parse_panel_float(char *text, float &out) {
  for (char *p = text; *p != '\0'; p++) {
    if (*p == ',')
      *p = '.';
  }
  char *end;
  out = strtof(text, &end);
  return end != text;
}

bool parse_panel_int(const char *text, int &out) {
  char *end;
  out = static_cast<int>(strtol(text, &end, 10));
  return end != text;
}

static bool is_padding(char c) { return c == ' ' || c == '\r' || c == '\n'; }

static bool matches(const char *text, size_t len, const char *spelling) {
  return strlen(spelling) == len && strncasecmp(text, spelling, len) == 0;
}

bool parse_panel_bool(const char *text, bool &out) {
  static constexpr const char *TRUE_SPELLINGS[] = {"1", "01", "on", "true"};
  static constexpr const char *FALSE_SPELLINGS[] = {"0", "00", "off", "false"};
  while (is_padding(*text))
    text++;
  size_t len = strlen(text);
  while (len > 0 && is_padding(text[len - 1]))
    len--;
  for (const char *spelling : TRUE_SPELLINGS) {
    if (matches(text, len, spelling)) {
      out = true;
      return true;
    }
  }
  for (const char *spelling : FALSE_SPELLINGS) {
    if (matches(text, len, spelling)) {
      out = false;
      return true;
    }
  }
  return false;
}

}  // namespace esphome::fendt_caravan
