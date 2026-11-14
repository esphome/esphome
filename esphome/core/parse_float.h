#pragma once

#include <cstdint>

#include "esphome/core/optional.h"

namespace esphome {

/// Parse a decimal floating-point number; the whole string must be the number.
optional<float> parse_float(const char *str);

/// strtof with picolibc's results on every platform; use it instead of strtof, strtod or atof.
float str_to_float(const char *str, char **end);

}  // namespace esphome
