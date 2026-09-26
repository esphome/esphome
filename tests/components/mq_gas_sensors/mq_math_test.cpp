#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>

#include "esphome/components/mq_gas_sensors/mq_math.h"

namespace esphome::mq_gas_sensors::testing {

namespace {

/// MQ-8 hydrogen curve of the reference library.
constexpr float MQ8_A = 976.97f;
constexpr float MQ8_B = -0.688f;

/// Half scale of a 12 bit ADC at 3.3 V and the RS it produces in clean air (VCC 5 V, RL 10 kOhm).
constexpr double ADC_HALF_V = 2048.0 * 3.3 / 4095.0;
constexpr double RS_CLEAN_AIR = (5.0 * 10.0) / ADC_HALF_V - 10.0;

/// MQ-8 temperature/humidity constants (a, b and c at RH 33 % and RH 85 %).
constexpr TcCorrectionCoefficients MQ8_TC{0.8559f, -0.0611f, 0.1673f, 0.8201f, -0.0606f, 0.1492f};

/// ppm scales with correction**|b'|, i.e. correction**(1 / 1.4494) = correction**0.68994.
constexpr double TC_SCALE_EXPONENT = 0.68994;

/// Absolute tolerance of the comparisons with a documented expected value.
constexpr double EPS = 1e-4;

}  // namespace

// The expected values are recomputed here with plain <cmath> calls, so these tests
// double as a check that the ported formulas still match MQUnifiedsensor and the
// MQDataScience correction.

TEST(MQMathVoltage, CountsToVolt) {
  EXPECT_NEAR(voltage_from_adc(4095, 12, 3.3f), 3.3, EPS);
  EXPECT_NEAR(voltage_from_adc(2048, 12, 3.3f), 2048.0 * 3.3 / 4095.0, EPS);
  EXPECT_NEAR(voltage_from_adc(1023, 10, 5.0f), 5.0, EPS);
}

TEST(MQMathResistance, RsFromVoltage) {
  EXPECT_NEAR(rs_from_voltage(static_cast<float>(ADC_HALF_V), 5.0f, 10.0f), (5.0 * 10.0) / ADC_HALF_V - 10.0, EPS);
  EXPECT_FLOAT_EQ(rs_from_voltage(0.0f, 5.0f, 10.0f), 0.0f);
  EXPECT_FLOAT_EQ(rs_from_voltage(-1.0f, 5.0f, 10.0f), 0.0f);
}

TEST(MQMathPpm, MQ8HydrogenCurve) {
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, 1.0f, REGRESSION_EXPONENTIAL), MQ8_A, EPS);
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, 70.0f, REGRESSION_EXPONENTIAL), 976.97 * std::pow(70.0, -0.688), EPS);
  // The documented clean air reading at a ratio of 70.
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, 70.0f, REGRESSION_EXPONENTIAL), 52.47, 0.5);
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, 0.5f, REGRESSION_EXPONENTIAL), 976.97 * std::pow(0.5, -0.688), EPS);
}

TEST(MQMathCalibration, R0FromCleanAir) {
  const float rs_air = static_cast<float>(RS_CLEAN_AIR);
  const float r0 = r0_from_clean_air(rs_air, 70.0f, 0.0f);
  EXPECT_NEAR(r0, RS_CLEAN_AIR / 70.0, EPS);
  EXPECT_NEAR(ratio_from_rs(rs_air, r0, 0.0f, RATIO_RS_R0), 70.0, 1e-3);
  // A calibrated sensor reads the clean air value again, not another curve point.
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, ratio_from_rs(rs_air, r0, 0.0f, RATIO_RS_R0), REGRESSION_EXPONENTIAL), 52.47,
              0.5);
  EXPECT_FLOAT_EQ(r0_from_clean_air(0.0f, 70.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(r0_from_clean_air(10.0f, 0.0f, 0.0f), 0.0f);
}

TEST(MQMathRatio, R0RsMode) {
  const float rs_air = static_cast<float>(RS_CLEAN_AIR);
  const float r0 = r0_from_clean_air(rs_air, 70.0f, 0.0f);
  EXPECT_NEAR(ratio_from_rs(rs_air, r0, 0.0f, RATIO_R0_RS), 1.0 / 70.0, 1e-6);
}

TEST(MQMathPpm, LinearRegression) {
  // MQ-131 O3.
  const float a = 0.41195f;
  const float b = -0.4708f;
  EXPECT_NEAR(ppm_from_ratio(a, b, 1.0f, REGRESSION_LINEAR), std::pow(10.0, (std::log10(1.0) + 0.4708) / 0.41195), EPS);
  EXPECT_NEAR(ppm_from_ratio(a, b, 1.0f, REGRESSION_LINEAR), 13.9, 0.1);
}

TEST(MQMathPpm, LinearRegressionWithNegativeA) {
  // MQ-135 NH3.
  EXPECT_NEAR(ppm_from_ratio(-0.47712f, 0.4491f, 1.0f, REGRESSION_LINEAR), std::pow(10.0, (0.0 - 0.4491) / -0.47712),
              EPS);
}

TEST(MQMathPpm, GuardsAndOverflow) {
  EXPECT_FLOAT_EQ(ppm_from_ratio(MQ8_A, MQ8_B, 0.0f, REGRESSION_EXPONENTIAL), 0.0f);
  EXPECT_FLOAT_EQ(ppm_from_ratio(0.0f, MQ8_B, 3.0f, REGRESSION_EXPONENTIAL), 0.0f);
  // MQ-3 CH4 curve (a = 2e31, b = 19.01): 2e31 * 10^19.01 exceeds FLT_MAX.
  EXPECT_FLOAT_EQ(ppm_from_ratio(2e31f, 19.01f, 10.0f, REGRESSION_EXPONENTIAL), FLT_MAX);
  // b = max with a tiny ratio: 10^-3800 underflows to 0.
  EXPECT_FLOAT_EQ(ppm_from_ratio(1.0f, 100.0f, 1e-38f, REGRESSION_EXPONENTIAL), 0.0f);
  EXPECT_DOUBLE_EQ(safe_pow(3.0, 0.0), 1.0);
  EXPECT_DOUBLE_EQ(safe_pow(3.0, 2.0), 9.0);
  EXPECT_TRUE(will_overflow(1e6));
  EXPECT_FALSE(will_overflow(2.0));
}

TEST(MQMathClamp, Coefficients) {
  EXPECT_FLOAT_EQ(clamp_a(NAN), 0.0f);
  EXPECT_FLOAT_EQ(clamp_a(static_cast<float>(MQ_MAX_A * 10.0)), static_cast<float>(MQ_MAX_A));
  EXPECT_FLOAT_EQ(clamp_a(static_cast<float>(-MQ_MAX_A * 10.0)), static_cast<float>(-MQ_MAX_A));
  EXPECT_FLOAT_EQ(clamp_b(static_cast<float>(MQ_MAX_B * 10.0)), static_cast<float>(MQ_MAX_B));
}

TEST(MQMathClamp, Ppm) {
  EXPECT_FLOAT_EQ(clamp_ppm(50000.0f, 0.0f, 10000.0f), 10000.0f);
  EXPECT_FLOAT_EQ(clamp_ppm(FLT_MAX, 0.0f, 10000.0f), 10000.0f);
  EXPECT_FLOAT_EQ(clamp_ppm(-1.0f, 0.0f, 10000.0f), 0.0f);
  EXPECT_TRUE(std::isnan(clamp_ppm(NAN, 0.0f, 10000.0f)));
}

TEST(MQMathPpm, MQDataScienceInverseRegression) {
  // Their MQ-8 H2 curve: a = 18391.5667, b = -1.4494, PPM = (ratio / a)^(1 / b).
  const float ds_a = 18391.5667f;
  const float ds_b = -1.4494f;
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 70.0f, REGRESSION_INVERSE), std::pow(70.0 / 18391.5667, 1.0 / -1.4494), EPS);
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 70.0f, REGRESSION_INVERSE), 46.7, 0.5);
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 10.0f, REGRESSION_INVERSE), std::pow(10.0 / 18391.5667, 1.0 / -1.4494), EPS);
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 1.0f, REGRESSION_INVERSE), 875.4, 5.0);
  // A' = a^(-1/b) and b' = 1/b, so the inverse form is an exponential curve.
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 70.0f, REGRESSION_INVERSE), 875.4 * std::pow(70.0, -0.68994), 0.1);
  EXPECT_NEAR(ppm_from_ratio(ds_a, ds_b, 1.0f, REGRESSION_INVERSE), 875.4 * std::pow(1.0, -0.68994), 0.5);
  // The two published MQ-8 datasets differ by ~11 % in clean air.
  EXPECT_NEAR(100.0 * (ppm_from_ratio(ds_a, ds_b, 70.0f, REGRESSION_INVERSE) /
                           ppm_from_ratio(MQ8_A, MQ8_B, 70.0f, REGRESSION_EXPONENTIAL) -
                       1.0),
              -11.1, 0.2);
  EXPECT_FLOAT_EQ(ppm_from_ratio(ds_a, ds_b, 0.0f, REGRESSION_INVERSE), 0.0f);
  EXPECT_FLOAT_EQ(ppm_from_ratio(0.0f, ds_b, 3.0f, REGRESSION_INVERSE), 0.0f);
}

TEST(MQMathCorrection, LinearInterpolation) {
  EXPECT_FLOAT_EQ(linear_interpolate(33.0f, 33.0f, 85.0f, 1.0f, 3.0f), 1.0f);
  EXPECT_FLOAT_EQ(linear_interpolate(85.0f, 33.0f, 85.0f, 1.0f, 3.0f), 3.0f);
  EXPECT_FLOAT_EQ(linear_interpolate(59.0f, 33.0f, 85.0f, 1.0f, 3.0f), 2.0f);
  EXPECT_FLOAT_EQ(linear_interpolate(59.0f, 33.0f, 33.0f, 1.0f, 3.0f), 1.0f);
}

TEST(MQMathCorrection, Coefficient) {
  EXPECT_NEAR(correction_coefficient(40.0f, 20.0f, MQ8_TC), 0.8997, 1e-3);
  // At the RH midpoint (59 %) a, b and c are the arithmetic means of the table.
  EXPECT_NEAR(correction_coefficient(59.0f, 20.0f, MQ8_TC),
              (0.8559 + 0.8201) / 2.0 + 0.5 * (0.1673 + 0.1492) * std::exp(0.5 * (-0.0611 - 0.0606) * 20.0), 1e-3);
  EXPECT_NEAR(correction_coefficient(33.0f, 20.0f, MQ8_TC), 0.8559 + 0.1673 * std::exp(-0.0611 * 20.0), 1e-5);
  EXPECT_NEAR(correction_coefficient(85.0f, 20.0f, MQ8_TC), 0.8201 + 0.1492 * std::exp(-0.0606 * 20.0), 1e-5);
  // Humidity and temperature are clamped to the tabulated domain.
  EXPECT_FLOAT_EQ(correction_coefficient(10.0f, 20.0f, MQ8_TC), correction_coefficient(33.0f, 20.0f, MQ8_TC));
  EXPECT_FLOAT_EQ(correction_coefficient(120.0f, 20.0f, MQ8_TC), correction_coefficient(85.0f, 20.0f, MQ8_TC));
  EXPECT_FLOAT_EQ(correction_coefficient(50.0f, -40.0f, MQ8_TC), correction_coefficient(50.0f, -10.0f, MQ8_TC));
  EXPECT_FLOAT_EQ(correction_coefficient(50.0f, 80.0f, MQ8_TC), correction_coefficient(50.0f, 50.0f, MQ8_TC));
  // A missing or failed ambient reading must never change the measurement.
  EXPECT_FLOAT_EQ(correction_coefficient(NAN, 20.0f, MQ8_TC), 1.0f);
  EXPECT_FLOAT_EQ(correction_coefficient(40.0f, NAN, MQ8_TC), 1.0f);
  EXPECT_FLOAT_EQ(correction_coefficient(40.0f, INFINITY, MQ8_TC), 1.0f);
}

TEST(MQMathCorrection, ApplyCorrection) {
  EXPECT_NEAR(apply_correction(70.0f, 0.9f), 70.0 / 0.9, EPS);
  EXPECT_FLOAT_EQ(apply_correction(70.0f, 1.0f), 70.0f);
  EXPECT_FLOAT_EQ(apply_correction(70.0f, 0.0f), 70.0f);
  EXPECT_FLOAT_EQ(apply_correction(70.0f, NAN), 70.0f);
  EXPECT_TRUE(std::isnan(apply_correction(NAN, 0.9f)));

  const float corr = correction_coefficient(40.0f, 20.0f, MQ8_TC);
  // Clean air (ratio 70) at RH 40 % / 20 degC gives ~48.8 instead of 52.5 ppm.
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, apply_correction(70.0f, corr), REGRESSION_EXPONENTIAL),
              976.97 * std::pow(70.0 / 0.8997, -0.688), 0.5);
  // The ppm scaling exponent used by the documentation is |b'| = 0.68994.
  EXPECT_NEAR(100.0 * (std::pow(static_cast<double>(corr), TC_SCALE_EXPONENT) - 1.0), -7.0, 0.2);
  // An uncorrected reading must be the regression alone.
  EXPECT_NEAR(ppm_from_ratio(MQ8_A, MQ8_B, apply_correction(70.0f, 1.0f), REGRESSION_EXPONENTIAL),
              976.97 * std::pow(70.0, -0.688), EPS);
}

TEST(MQMathClamp, CorrectedPpm) {
  // absolute: the alarm ceiling never moves, scaled: the ceiling follows the correction.
  EXPECT_FLOAT_EQ(clamp_ppm_corrected(50000.0f, 0.0f, 10000.0f, 0.9f, CLAMP_ABSOLUTE), 10000.0f);
  EXPECT_FLOAT_EQ(clamp_ppm_corrected(50000.0f, 0.0f, 10000.0f, 0.9f, CLAMP_SCALED), 9000.0f);
  EXPECT_FLOAT_EQ(clamp_ppm_corrected(5000.0f, 0.0f, 10000.0f, 0.9f, CLAMP_SCALED), 5000.0f);
}

}  // namespace esphome::mq_gas_sensors::testing
