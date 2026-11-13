#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MAX31855_LINEARIZE

namespace esphome::max31855 {

/// NIST ITS-90 range of the type K inverse polynomials, in mV.
static constexpr float K_TYPE_MV_MIN = -5.891f;
static constexpr float K_TYPE_MV_MAX = 54.886f;

/// Type K thermoelectric voltage in mV for a temperature in °C (NIST ITS-90).
float k_type_mv(float t);

/// Type K temperature in °C for a thermoelectric voltage in mV (NIST ITS-90
/// inverse). NAN outside K_TYPE_MV_MIN..K_TYPE_MV_MAX.
float k_type_celsius(float mv);

/// Correct a MAX31855 reading. The chip reports t_chip = t_cj + V_tc / 41.276 uV/°C,
/// a straight-line approximation of the type K curve. Recover V_tc, add the NIST
/// voltage of the cold junction, and convert the sum back with the NIST inverse.
/// NAN when the result falls outside the NIST range.
float k_type_linearize(float t_chip, float t_cj);

}  // namespace esphome::max31855

#endif  // USE_MAX31855_LINEARIZE
