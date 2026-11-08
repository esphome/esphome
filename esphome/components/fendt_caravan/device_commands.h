#pragma once

#ifdef USE_ESP32
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace esphome::fendt_caravan {

/// Buffer size for one command, including the terminating NUL
static constexpr size_t COMMAND_MAX = 48;

/// Builds panel commands into a caller-owned buffer of COMMAND_MAX bytes.
/// Each returns `buf`, or nullptr when the command does not fit.
class Commands {
 public:
  static const char *update_toggle(char *buf, const char *name) { return format_(buf, "cmd-tgl:%s", name); }

  static const char *update_run(char *buf, const char *name) {
    if (strcmp(name, "HS_KEY") == 0 || strcmp(name, "HS_KEY_LONG") == 0)
      return format_(buf, "net-%s", name);
    return format_(buf, "cmd-run:%s", name);
  }

 protected:
  static const char *format_(char *buf, const char *format, const char *name) {
    const int len = snprintf(buf, COMMAND_MAX, format, name);
    return len > 0 && static_cast<size_t>(len) < COMMAND_MAX ? buf : nullptr;
  }
};

}  // namespace esphome::fendt_caravan
#endif
