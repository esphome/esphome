#pragma once

#include <cstdint>

#include "esphome/core/optional.h"

namespace esphome {

/// Parse a decimal floating-point number; the whole string must be the number.
optional<float> parse_float(const char *str);

/** strtof without libc's strtod: picolibc's strtof (1.8.10), bit for bit and with its end pointer, on every
 * platform.
 *
 * Use it instead of strtof, (float) strtod or atof, which link libc's float parser.
 * @param str String to parse.
 * @param end If not null, set to the first character not parsed, or to \p str when nothing was.
 */
float str_to_float(const char *str, char **end);

}  // namespace esphome
