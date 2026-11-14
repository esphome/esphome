#pragma once

#include <cstdint>

#include "esphome/core/optional.h"

namespace esphome {

/// The whole string as str_to_float's syntax (leading space, hex, inf, nan); trailing characters, "inf" and positive
/// overflow are rejected, while -inf, negative overflow and NaN are accepted.
optional<float> parse_float(const char *str);

/// strtof with picolibc's results on every platform; use it instead of strtof, strtod or atof.
float str_to_float(const char *str, char **end);

}  // namespace esphome
