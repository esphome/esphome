/*
 * Linker wrap stub for sscanf on ESP-IDF, newlib only.
 *
 * Nothing in ESPHome calls sscanf, but bluedroid does in two places:
 * btc_config_get_bin decodes stored bonding keys with "%02x" and
 * string_to_bdaddr parses "%02x:%02x:%02x:%02x:%02x:%02x". Those two
 * calls are the only reference to newlib's scanf engine (__ssvfscanf_r,
 * _strtod_l and their helpers, ~13 KB) in a Bluetooth build on variants
 * whose ROM does not export sscanf.
 *
 * This stub routes sscanf through the scanner in core/sscanf_no_float.h,
 * so any other caller in the image (user lambdas included) keeps working.
 * Floating-point conversions are the one gap: supporting them would pull
 * _strtod_l straight back, so a format using %f and friends aborts with a
 * message pointing at enable_full_scanf: true. Codegen already keeps the
 * libc sscanf when a lambda scans a float.
 *
 * Only compiled in when codegen defines USE_ESP32_SSCANF_STUB, which is
 * gated on a Bluetooth component being present, on the variant's ROM and
 * on the same newlib condition as printf_stubs.cpp.
 */

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_SSCANF_STUB)

#include <cstdarg>

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
    esp_system_abort("sscanf: unsupported conversion; set enable_full_scanf: true in esp32 framework advanced config");
  }
  return result;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF && USE_ESP32_SSCANF_STUB
