#pragma once

#include <cstdint>

#include "esphome/core/optional.h"

namespace esphome {

/// Parse a decimal floating-point number; the whole string must be the number.
optional<float> parse_float(const char *str);

/** strtof without libc's strtod: the same value and end as newlib's strtof in the C locale for every input.
 *
 * Use it instead of strtof, (float) strtod or atof, which link libc's float parser.
 * @param str String to parse.
 * @param end If not null, set to the first character not parsed, or to \p str when nothing was.
 */
float parse_float_prefix(const char *str, char **end);

namespace internal {
/// The bits of the float nearest to a non-negative double (ties to even), without the platform's conversion,
/// which flushes float denormals to zero on some chips (RP2350).
uint32_t double_to_float_bits(double value);
#ifdef USE_HOST
/// Exact comparison rounds run so far, for tests to check the bound
uint32_t parse_float_exact_steps();
#endif
}  // namespace internal

}  // namespace esphome
