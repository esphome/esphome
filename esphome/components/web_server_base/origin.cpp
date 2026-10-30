#include "origin.h"

#include <cstring>

namespace esphome::web_server_base {

bool is_origin_allowed(const char *origin, size_t origin_len, const char *host, size_t host_len,
                       const char *const *allowed) {
  if (origin_len == 0)
    return true;

  if (origin != nullptr && host_len != 0) {
    // Same-origin: covers the device's IP, mDNS name or DNS name without knowing any at compile time.
    for (size_t i = 0; i + 3 <= origin_len; i++) {
      if (origin[i] == ':' && origin[i + 1] == '/' && origin[i + 2] == '/') {
        const size_t authority = i + 3;
        if (origin_len - authority == host_len && memcmp(origin + authority, host, host_len) == 0)
          return true;
        break;
      }
    }
  }

  if (allowed == nullptr)
    return false;
  for (; *allowed != nullptr; allowed++) {
    const char *entry = *allowed;
    if (entry[0] == '*' && entry[1] == '\0')
      return true;
    if (origin != nullptr && strncmp(entry, origin, origin_len) == 0 && entry[origin_len] == '\0')
      return true;
  }
  return false;
}

}  // namespace esphome::web_server_base
