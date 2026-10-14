/*
 * Linker wrap stubs for FILE*-based printf functions.
 *
 * The ESP8266 Arduino framework and libraries may reference printf(),
 * vprintf(), and fprintf() which pull in newlib's _vfprintf_r (~900 bytes).
 * ESPHome never uses these — all logging writes directly to the UART via
 * Arduino's Serial, so the libc FILE*-based printf path is dead code.
 *
 * These stubs redirect through vsnprintf() (which is already in the binary
 * for ESPHome's logging) and fwrite(), allowing the linker to dead-code
 * eliminate _vfprintf_r.
 *
 * Saves ~1.6 KB of flash.
 */

#if defined(USE_ESP8266) && !defined(USE_FULL_PRINTF)
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace esphome::esp8266 {}

static constexpr size_t PRINTF_BUFFER_SIZE = 512;

// These stubs are essentially dead code at runtime — ESPHome writes directly
// to the UART via Arduino's Serial, and Serial.printf() has its own implementation.
// The buffer overflow check is purely defensive and should never trigger.
static int write_printf_buffer(FILE *stream, char *buf, int len) {
  if (len < 0) {
    return len;
  }
  size_t write_len = len;
  if (write_len >= PRINTF_BUFFER_SIZE) {
    fwrite(buf, 1, PRINTF_BUFFER_SIZE - 1, stream);
    abort();
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

#endif  // USE_ESP8266 && !USE_FULL_PRINTF
