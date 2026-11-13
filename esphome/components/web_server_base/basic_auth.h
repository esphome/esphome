#pragma once

#include <cstddef>

namespace esphome::web_server_base {

/** Whether Basic credentials from an Authorization header equal the expected base64 hash.
 *
 * `credentials` is what follows the scheme and its first space. Further leading spaces (RFC 9110 allows
 * 1*SP) and trailing spaces or tabs are ignored; the rest must match exactly, base64 being case-sensitive.
 * Equal lengths are compared in constant time, so a wrong hash cannot be found byte by byte from timing.
 */
bool basic_auth_matches(const char *credentials, size_t credentials_len, const char *expected, size_t expected_len);

}  // namespace esphome::web_server_base
