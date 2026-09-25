/*
 * Linker wrap stubs for FILE*-based printf functions (newlib only).
 *
 * ESP-IDF SDK components (gpio driver, ringbuf, log_write) reference
 * fprintf(), printf(), vprintf(), and vfprintf(), which on newlib pull
 * in _vfprintf_r (~11 KB) — a separate implementation from the one used
 * by snprintf/vsnprintf that handles FILE* stream I/O with buffering.
 *
 * ESPHome replaces the ESP-IDF log handler via esp_log_set_vprintf_(),
 * so the SDK's vprintf() path is dead code at runtime. The fprintf()
 * and printf() calls in SDK components are only in debug/assert paths
 * (gpio_dump_io_configuration, ringbuf diagnostics) that are either
 * GC'd or never called. Crash backtraces and panic output are
 * unaffected; they use esp_rom_printf() which is a ROM function and
 * does not go through libc.
 *
 * This wrap is newlib-only. On picolibc, vsnprintf is implemented as
 * vfprintf into a string-output FILE, so vfprintf is unconditionally
 * linked in by any caller of snprintf/vsnprintf and the wrap can never
 * elide it — it just adds shim cost. Codegen forces USE_FULL_PRINTF
 * on picolibc builds (IDF 6.0+ on all variants) so this file compiles
 * to nothing there; the #error below catches a desynchronised gate.
 *
 * Saves ~11 KB of flash on newlib.
 *
 * To disable this wrap on newlib, set enable_full_printf: true in the
 * esp32 advanced config section.
 */

#if defined(USE_ESP_IDF) && !defined(USE_FULL_PRINTF)

#ifdef __PICOLIBC__
#error "printf wrap is net-negative on picolibc; codegen should set USE_FULL_PRINTF"
#endif

#include <cstdarg>
#include <cstdio>

#include "esp_system.h"

namespace esphome::esp32 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

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

int __wrap_vprintf(const char *fmt, va_list ap) {
  char buf[PRINTF_BUFFER_SIZE];
  return write_printf_buffer(stdout, buf, vsnprintf(buf, sizeof(buf), fmt, ap));
}

int __wrap_vfprintf(FILE *stream, const char *fmt, va_list ap) {
  char buf[PRINTF_BUFFER_SIZE];
  return write_printf_buffer(stream, buf, vsnprintf(buf, sizeof(buf), fmt, ap));
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
  int len = __wrap_vfprintf(stream, fmt, ap);
  va_end(ap);
  return len;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && !USE_FULL_PRINTF
