/*
 * Linker wrap for sscanf on ESP-IDF with newlib.
 *
 * bluedroid's two "%02x" parses are the only sscanf callers in an ESPHome
 * image and link newlib's whole scanf engine (~13 KB with _strtod_l). Routing
 * them through core/sscanf_no_float.h drops it; floats would pull _strtod_l
 * back, so those abort naming enable_full_scanf. See _add_sscanf_stub in
 * esp32/__init__.py for when this is emitted.
 */

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_SSCANF_STUB)

#include <cstdarg>
#include <cstdio>

#include "esp_system.h"
#include "esphome/core/sscanf_no_float.h"

namespace esphome::esp32 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

int __wrap_sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int result = esphome::vsscanf_no_float(str, fmt, ap);
  va_end(ap);
  if (result == esphome::SSCANF_UNSUPPORTED) {
    char msg[160];
    snprintf(msg, sizeof(msg),
             "sscanf: unsupported conversion in \"%s\"; set enable_full_scanf: true in esp32 framework", fmt);
    esp_system_abort(msg);
  }
  return result;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && USE_ESP32_SSCANF_STUB
