#include "origin.h"

#include <cstring>

#include "esphome/core/string_ref.h"

namespace esphome::web_server_base {

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
      if (authority <= origin_len && colon[1] == '/' && colon[2] == '/' &&
          StringRef(origin + authority, origin_len - authority) == StringRef(host, host_len))
        return true;
    }
  }

  if (allowed == nullptr)
    return false;
  const StringRef origin_ref(origin, origin_len);
  for (; *allowed != nullptr; allowed++) {
    if (origin_ref == *allowed)
      return true;
  }
  return false;
}

}  // namespace esphome::web_server_base
