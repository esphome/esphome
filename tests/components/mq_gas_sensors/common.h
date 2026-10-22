#pragma once

#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/mq_gas_sensors/mq_math.h"

namespace esphome::mq_gas_sensors::testing {

using mqmath::apply_correction;
using mqmath::calibration_is_stable;
using mqmath::clamp_ppm;
using mqmath::clamp_ppm_corrected;
using mqmath::correction_coefficient;
using mqmath::ppm_from_ratio;
using mqmath::r0_from_clean_air;
using mqmath::ratio_from_rs;
using mqmath::rs_from_voltage;
using mqmath::CLAMP_ABSOLUTE;
using mqmath::CLAMP_SCALED;
using mqmath::REGRESSION_EXPONENTIAL;
using mqmath::REGRESSION_INVERSE;
using mqmath::REGRESSION_LINEAR;
using mqmath::RATIO_RS_R0;
using mqmath::RATIO_R0_RS;
using mqmath::RegressionMethod;

// A fixture of the built-in table: independent copies, so a change in
// `coefficients.py` cannot quietly change what these tests mean (the Python
// tests pin that table itself).
constexpr float H2_A = 976.97f;  // MQ-8 H2, exponential
constexpr float H2_B = -0.688f;
constexpr float H2_MAX_PPM = 10000.0f;
constexpr float NH3_A = -0.47712f;  // MQ-135 NH3, linear
constexpr float NH3_B = 0.4491f;

// The MQ-8 temperature/humidity correction constants of Correction.cpp.
constexpr mqmath::TcCorrectionCoefficients MQ8_TC{0.8559f, -0.0611f, 0.1673f, 0.8201f, -0.0606f, 0.1492f};

}  // namespace esphome::mq_gas_sensors::testing
