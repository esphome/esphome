#pragma once

#include <cstdint>

#include "esphome/core/optional.h"

namespace esphome {

/// The whole string as str_to_float's syntax (leading space, hex, inf, nan); trailing characters, "inf" and positive
/// overflow are rejected, while -inf, negative overflow and NaN are accepted.
optional<float> parse_float(const char *str);

/// strtof with picolibc's results on every platform, but it never sets errno (overflow gives +-HUGE_VALF, underflow 0);
/// use it instead of strtof, or strtod/atof for a float result.
float str_to_float(const char *str, char **end);

}  // namespace esphome
