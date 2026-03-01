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
 * GC'd or never called. Crash backtraces and panic output are
 * unaffected — they use esp_rom_printf() which is a ROM function
 * and does not go through libc.
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

namespace esphome::esp32 {}

static constexpr size_t PRINTF_BUFFER_SIZE = 512;

// These stubs are essentially dead code at runtime — ESPHome replaces the
// ESP-IDF log handler, and the SDK's printf/fprintf calls only exist in
// debug/assert paths that are never reached in normal operation.
// The buffer overflow check is purely defensive and should never trigger.
static int write_printf_buffer(FILE *stream, char *buf, int len) {
  if (len < 0) {
    return len;
  }
  size_t write_len = len;
  if (write_len >= PRINTF_BUFFER_SIZE) {
    fwrite(buf, 1, PRINTF_BUFFER_SIZE - 1, stream);
    esp_system_abort("printf buffer overflow; set enable_full_printf: true in esp32 framework advanced config");
  }
  if (fwrite(buf, 1, write_len, stream) < write_len || ferror(stream)) {
    return -1;
  }
  return len;
}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

int __wrap_vprintf(const char *fmt, va_list ap) {
  char buf[PRINTF_BUFFER_SIZE];
  return write_printf_buffer(stdout, buf, vsnprintf(buf, sizeof(buf), fmt, ap));
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
  int len = write_printf_buffer(stream, buf, vsnprintf(buf, sizeof(buf), fmt, ap));
  va_end(ap);
  return len;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && !USE_FULL_PRINTF
