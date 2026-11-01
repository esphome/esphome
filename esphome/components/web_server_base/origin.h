#pragma once

#include <cstddef>

namespace esphome::web_server_base {

/// Longest Origin or Host value the ESP32 server reads into its stack buffers; keep in sync with
/// MAX_ORIGIN_LENGTH in web_server_base/__init__.py.
static constexpr size_t MAX_ORIGIN_LENGTH = 127;

/** Decide whether a browser request from the given Origin may reach a handler.
 *
 * A request without an Origin (origin_len == 0) is not a cross-origin browser request and is allowed.
 * A same-origin request, whose Origin authority (the part after "scheme://") equals the Host header,
 * is allowed. Any other origin must exactly equal an entry of the nullptr-terminated allowed table
 * (nullptr when none is configured). A configured "*" never reaches this table: codegen turns it into
 * USE_WEBSERVER_ALLOW_ANY_ORIGIN, which skips the check entirely.
 *
 * @param host Host header value; host_len == 0 when absent or unreadable.
 */
bool is_origin_allowed(const char *origin, size_t origin_len, const char *host, size_t host_len,
                       const char *const *allowed);

}  // namespace esphome::web_server_base
