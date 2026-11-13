#include "basic_auth.h"

#include <cstdint>

namespace esphome::web_server_base {

bool basic_auth_matches(const char *credentials, size_t credentials_len, const char *expected, size_t expected_len) {
  while (credentials_len != 0 && *credentials == ' ') {
    credentials++;
    credentials_len--;
  }
  while (credentials_len != 0 && (credentials[credentials_len - 1] == ' ' || credentials[credentials_len - 1] == '\t'))
    credentials_len--;
  if (expected_len == 0 || credentials_len != expected_len)
    return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < expected_len; i++)
    diff |= static_cast<uint8_t>(credentials[i] ^ expected[i]);
  return diff == 0;
}

}  // namespace esphome::web_server_base
