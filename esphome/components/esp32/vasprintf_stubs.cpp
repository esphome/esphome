/*
 * Linker wrap stub for vasprintf() on variants whose ROM exports a
 * full-format vsnprintf() but no vasprintf() (ESP32-C6, newlib only).
 *
 * On those chips every snprintf/vsnprintf call in the image resolves to
 * the ROM, so the newlib printf engine (_svfprintf_r, _dtoa_r and their
 * helpers, ~20 KB) is not linked at all until something references a
 * printf-family function the ROM lacks. esp_http_client does exactly that
 * through vasprintf() in its header and auth helpers, so adding
 * http_request to a build costs the whole engine on top of the HTTP and
 * TLS code itself.
 *
 * This stub reimplements vasprintf() on top of the ROM vsnprintf(), which
 * keeps the engine out of the image. It is only compiled in when codegen
 * defines USE_ESP32_VASPRINTF_STUB, which is gated on the variant's ROM
 * linker script and on the same newlib condition as printf_stubs.cpp.
 */

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_VASPRINTF_STUB)

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace esphome::esp32 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

int __wrap_vasprintf(char **strp, const char *fmt, va_list ap) {
  va_list ap_copy;
  va_copy(ap_copy, ap);
  int len = vsnprintf(nullptr, 0, fmt, ap_copy);
  va_end(ap_copy);
  if (len < 0) {
    return len;
  }
  char *buf = static_cast<char *>(malloc(static_cast<size_t>(len) + 1));
  if (buf == nullptr) {
    return -1;
  }
  vsnprintf(buf, static_cast<size_t>(len) + 1, fmt, ap);
  *strp = buf;
  return len;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && USE_ESP32_VASPRINTF_STUB
