/*
 * Linker wrap stubs for FILE*-based printf functions.
 *
 * ESP-IDF SDK components (gpio driver, ringbuf, log_write) reference
 * fprintf(), printf(), and vprintf() which pull in newlib's _vfprintf_r
 * (~11 KB). This is a separate implementation from _svfprintf_r (used by
 * snprintf/vsnprintf) that handles FILE* stream I/O with buffering and
 * locking.
 *
 * ESPHome replaces the ESP-IDF log handler via esp_log_set_vprintf_(),
 * so the SDK's vprintf() path is dead code at runtime. The fprintf()
 * and printf() calls in SDK components are only in debug/assert paths
 * (gpio_dump_io_configuration, ringbuf diagnostics) that are either
 * GC'd or never called.
 *
 * These stubs redirect through vsnprintf() (which uses _svfprintf_r
 * already in the binary) and fwrite(), allowing the linker to
 * dead-code eliminate _vfprintf_r.
 *
 * Saves ~11 KB of flash.
 *
 * To disable these wraps, set enable_full_printf: true in the esp32
 * advanced config section.
 */

#if defined(USE_ESP_IDF) && !defined(USE_FULL_PRINTF)
#include <cstdarg>
#include <cstdio>

#include "esp_system.h"

static constexpr size_t PRINTF_BUFFER_SIZE = 512;

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

int __wrap_vprintf(const char *fmt, va_list ap) {
  char buf[PRINTF_BUFFER_SIZE];
  int len = vsnprintf(buf, sizeof(buf), fmt, ap);
  if (len < 0) {
    return len;
  }
  if (static_cast<size_t>(len) >= sizeof(buf)) {
    // Output was truncated — this should not happen in normal operation.
    // Abort to make the issue visible rather than silently losing output.
    esp_system_abort("printf buffer overflow; set enable_full_printf: true in esp32 advanced config");
  }
  fwrite(buf, 1, len, stdout);
  return len;
}

int __wrap_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = __wrap_vprintf(fmt, ap);
  va_end(ap);
  return len;
}

int __wrap_fprintf(FILE *stream, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char buf[PRINTF_BUFFER_SIZE];
  int len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len < 0) {
    return len;
  }
  if (static_cast<size_t>(len) >= sizeof(buf)) {
    esp_system_abort("fprintf buffer overflow; set enable_full_printf: true in esp32 advanced config");
  }
  fwrite(buf, 1, len, stream);
  return len;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && !USE_FULL_PRINTF
