/*
 * Linker wrap for strtof().
 *
 * SDK code that calls strtof (the BK7238 calibration library) would otherwise link
 * newlib's strtod beside ESPHome's parser; on BK72xx chips with no such caller it is unused.
 */

#include "esphome/core/defines.h"

#ifdef USE_BK72XX

#include "esphome/core/parse_float.h"

namespace esphome::libretiny {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" float __wrap_strtof(const char *str, char **end) { return esphome::str_to_float(str, end); }
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_BK72XX
