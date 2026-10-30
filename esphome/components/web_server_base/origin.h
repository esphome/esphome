#pragma once

#include <cstddef>

namespace esphome::web_server_base {

/** Decide whether a browser request from the given Origin may reach a handler.
 *
 * A request without an Origin (origin_len == 0) is not a cross-origin browser request and is allowed.
 * A same-origin request, whose Origin authority (the part after "scheme://") equals the Host header,
 * is allowed. Any other origin must equal an entry of the nullptr-terminated allowed table (nullptr when
 * none is configured); a "*" entry allows any origin.
 *
 * @param host Host header value; host_len == 0 when absent or unreadable.
 */
bool is_origin_allowed(const char *origin, size_t origin_len, const char *host, size_t host_len,
                       const char *const *allowed);

/// True when the nullptr-terminated allowed table (nullptr when none is configured) has a "*" entry.
/// Decides an Origin too long to read, which no exact entry can match.
bool allows_any_origin(const char *const *allowed);

}  // namespace esphome::web_server_base
