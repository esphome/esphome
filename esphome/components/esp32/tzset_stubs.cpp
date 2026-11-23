/*
 * Linker wrap stub for picolibc's _tzset_unlocked().
 *
 * strftime() and mktime() call it to load the time zone from the TZ variable,
 * and its parser links the C library's scanf engine. ESPHome never sets TZ (it
 * parses the time zone itself and fills in %Z and %z), so the call would only
 * reapply the defaults tzname, _timezone and _daylight already have. An
 * explicit tzset() still parses TZ: its call is inside the same object. Code
 * that sets TZ must call tzset() itself; the lazy reload is skipped.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP32

namespace esphome::esp32 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" void __wrap__tzset_unlocked() {}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32
