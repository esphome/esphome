#include "origin.h"

#include <cstring>

namespace esphome::web_server_base {

static bool is_wildcard(const char *entry) { return entry[0] == '*' && entry[1] == '\0'; }

bool allows_any_origin(const char *const *allowed) {
  if (allowed == nullptr)
    return false;
  for (; *allowed != nullptr; allowed++) {
    if (is_wildcard(*allowed))
      return true;
  }
  return false;
}

bool is_origin_allowed(const char *origin, size_t origin_len, const char *host, size_t host_len,
                       const char *const *allowed) {
  if (origin_len == 0)
    return true;

  if (host_len != 0) {
    // Same-origin: covers the device's IP, mDNS name or DNS name without knowing any at compile time.
    // The scheme has no ':', so the first one starts "://".
    const char *colon = static_cast<const char *>(memchr(origin, ':', origin_len));
    if (colon != nullptr) {
      const size_t authority = colon - origin + 3;
      if (authority <= origin_len && colon[1] == '/' && colon[2] == '/' && origin_len - authority == host_len &&
          memcmp(origin + authority, host, host_len) == 0)
        return true;
    }
  }

  if (allowed == nullptr)
    return false;
  for (; *allowed != nullptr; allowed++) {
    const char *entry = *allowed;
    if (is_wildcard(entry))
      return true;
    if (strncmp(entry, origin, origin_len) == 0 && entry[origin_len] == '\0')
      return true;
  }
  return false;
}

}  // namespace esphome::web_server_base
