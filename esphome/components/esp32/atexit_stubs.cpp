/*
 * Linker wrap stub for __cxa_atexit().
 *
 * GCC registers the destructor of every global and function local static
 * object with __cxa_atexit(). The firmware never returns from main or calls
 * exit(), so these destructors never run, but the first registration links
 * newlib's exit handler tables (about 400 bytes of RAM on ESP32).
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP32

namespace esphome::esp32 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" int __wrap___cxa_atexit(void (*)(void *), void *, void *) { return 0; }
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32
