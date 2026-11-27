#pragma once

#include <cstdint>
#include <cstdlib>  // defines __PICOLIBC__ on picolibc

#include "esphome/core/optional.h"

namespace esphome {

/// The whole string as str_to_float's syntax (leading space, hex, inf, nan); trailing characters, "inf" and positive
/// overflow are rejected, while -inf, negative overflow and NaN are accepted.
optional<float> parse_float(const char *str);

/// strtof with picolibc's results on every platform; use it instead of strtof, or strtod/atof for a float result.
/// Where libc's strtof is smaller (picolibc, or USE_LIBC_STRTOF from the platform) it is that strtof; elsewhere it
/// never sets errno (overflow is +-HUGE_VALF, underflow 0).
#if defined(__PICOLIBC__) || defined(USE_LIBC_STRTOF)
#define ESPHOME_PARSE_FLOAT_LIBC
inline float str_to_float(const char *str, char **end) { return ::strtof(str, end); }
#else
float str_to_float(const char *str, char **end);
#endif

}  // namespace esphome
