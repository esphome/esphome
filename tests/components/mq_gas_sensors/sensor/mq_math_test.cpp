#include "../common.h"

namespace esphome::mq_gas_sensors::testing {

TEST(MqMath, SensorResistanceFollowsTheDivider) {
  // RS = (VCC * RL) / V - RL: half the supply across a 10 kOhm load is 10 kOhm.
  EXPECT_FLOAT_EQ(10.0f, rs_from_voltage(2.5f, 5.0f, 10.0f));
  // A shorted sensor (V at VCC) reads 0 kOhm, the saturated end of the range.
  EXPECT_FLOAT_EQ(0.0f, rs_from_voltage(5.0f, 5.0f, 10.0f));
  // A missing sensor (no output, or above the supply) reads unknown instead of a
  // resistance the divider would turn into a real measurement.
  EXPECT_TRUE(std::isnan(rs_from_voltage(0.0f, 5.0f, 10.0f)));
  EXPECT_TRUE(std::isnan(rs_from_voltage(6.0f, 5.0f, 10.0f)));
  EXPECT_TRUE(std::isnan(rs_from_voltage(NAN, 5.0f, 10.0f)));
}

TEST(MqMath, RatioIsRsOverR0) {
  EXPECT_FLOAT_EQ(1.0f, ratio_from_rs(10.0f, 10.0f, 0.0f, RATIO_RS_R0));
  EXPECT_FLOAT_EQ(7.0f, ratio_from_rs(70.0f, 10.0f, 0.0f, RATIO_RS_R0));
  // RS = 0 is a real reading (V_AO at VCC), the saturated end of the range.
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(0.0f, 10.0f, 0.0f, RATIO_RS_R0));
  // The same reading in the library convention: R0 / RS grows without bound, so
  // the ratio of 0 that reports the top of the range answers for both.
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(0.0f, 10.0f, 0.0f, RATIO_R0_RS));
  // Any other unusable input reads unknown, never a ratio the curve would turn
  // into an alarm reading at the top of the range.
  EXPECT_TRUE(std::isnan(ratio_from_rs(10.0f, 0.0f, 0.0f, RATIO_RS_R0)));
  EXPECT_TRUE(std::isnan(ratio_from_rs(NAN, 10.0f, 0.0f, RATIO_RS_R0)));
  EXPECT_TRUE(std::isnan(ratio_from_rs(-1.0f, 10.0f, 0.0f, RATIO_RS_R0)));
}

TEST(MqMath, CorrectionFactorMovesTheRatioOnly) {
  // The factor is added to the ratio ...
  EXPECT_FLOAT_EQ(1.5f, ratio_from_rs(10.0f, 10.0f, 0.5f, RATIO_RS_R0));
  // ... while R0 stays what clean air measured, so it is applied once.
  EXPECT_FLOAT_EQ(10.0f, r0_from_clean_air(700.0f, 70.0f));
  // A negative factor can push the ratio past zero at a real concentration. RS / R0
  // falls as the concentration rises, so the reading is above the range the curve
  // covers there: the saturated end (clamped to max_ppm), not unknown, which would
  // hide the reading instead.
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(10.0f, 10.0f, -1.0f, RATIO_RS_R0));
  const float saturated_ratio = ratio_from_rs(10.0f, 10.0f, -2.0f, RATIO_RS_R0);
  EXPECT_FLOAT_EQ(0.0f, saturated_ratio);
  const float saturated_ppm = ppm_from_ratio(H2_A, H2_B, saturated_ratio, REGRESSION_EXPONENTIAL);
  EXPECT_FLOAT_EQ(H2_MAX_PPM, clamp_ppm(saturated_ppm, 0.0f, H2_MAX_PPM));
  // R0 / RS rises with the concentration, so a ratio pushed past zero there sits
  // below the clean-air end of the curve: unknown, not an alarm.
  EXPECT_TRUE(std::isnan(ratio_from_rs(100.0f, 10.0f, -0.2f, RATIO_R0_RS)));
}

TEST(MqMath, CleanAirReadingGivesR0) {
  // RS/R0 in clean air is 70 for the MQ-8: 700 kOhm of RS is 10 kOhm of R0.
  EXPECT_FLOAT_EQ(10.0f, r0_from_clean_air(700.0f, 70.0f));
  EXPECT_FLOAT_EQ(0.0f, r0_from_clean_air(0.0f, 70.0f));
  EXPECT_FLOAT_EQ(0.0f, r0_from_clean_air(700.0f, 0.0f));
}

TEST(MqMath, MidRangeReadingsFollowTheCurve) {
  // 976.97 * 1^-0.688 = 976.97 ppm.
  EXPECT_NEAR(976.97f, ppm_from_ratio(H2_A, H2_B, 1.0f, REGRESSION_EXPONENTIAL), 0.01f);
  // 976.97 * 70^-0.688 = 52.5 ppm, the clean-air anchor of the MQ-8 H2 curve.
  EXPECT_NEAR(52.5f, ppm_from_ratio(H2_A, H2_B, 70.0f, REGRESSION_EXPONENTIAL), 0.1f);
  // Linear: log10(ppm) = (log10(1) - 0.4491) / -0.47712 = 0.9413 -> 8.73 ppm.
  EXPECT_NEAR(8.73f, ppm_from_ratio(NH3_A, NH3_B, 1.0f, REGRESSION_LINEAR), 0.01f);
}

TEST(MqMath, SaturatedSensorReportsTheTopOfTheRange) {
  // A ratio of 0 is RS at 0 (V_AO at VCC), the output of a saturated sensor:
  // the model reports the top of the range there, so it is clamped to max_ppm
  // instead of reading as clean air.
  EXPECT_EQ(FLT_MAX, ppm_from_ratio(H2_A, H2_B, 0.0f, REGRESSION_EXPONENTIAL));
  EXPECT_FLOAT_EQ(H2_MAX_PPM, clamp_ppm(ppm_from_ratio(H2_A, H2_B, 0.0f, REGRESSION_EXPONENTIAL), 0.0f, H2_MAX_PPM));
}

TEST(MqMath, UnusableInputReadsUnknown) {
  EXPECT_TRUE(std::isnan(ppm_from_ratio(H2_A, H2_B, NAN, REGRESSION_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(clamp_ppm(NAN, 0.0f, H2_MAX_PPM)));
  // Coefficients the method cannot evaluate (log10 of a negative a) read
  // unknown, not a saturated max_ppm.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(-H2_A, H2_B, 1.0f, REGRESSION_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(clamp_ppm(ppm_from_ratio(-H2_A, H2_B, 1.0f, REGRESSION_EXPONENTIAL), 0.0f, H2_MAX_PPM)));
  // No coefficients the method can use: a curve with a = 0 has no solution, so
  // it reads unknown rather than 0 ppm (clean air) or a clamped range value.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, REGRESSION_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, REGRESSION_LINEAR)));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, REGRESSION_INVERSE)));
}

TEST(MqMath, UnknownRegressionMethodReadsUnknown) {
  // Only the three defined methods have a formula; anything else must not be
  // evaluated with the linear one and published as a plausible reading.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(H2_A, H2_B, 1.0f, static_cast<RegressionMethod>(0))));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(H2_A, H2_B, 1.0f, static_cast<RegressionMethod>(4))));
}

TEST(MqMath, BelowTheNoiseFloorReadsZero) {
  // `min_ppm` is the noise floor: a reading below it counts as not detected and
  // reports 0 ppm, and 0 ppm itself is never lifted to `min_ppm`.
  EXPECT_FLOAT_EQ(0.0f, clamp_ppm(3.0f, 10.0f, H2_MAX_PPM));
  EXPECT_FLOAT_EQ(0.0f, clamp_ppm(0.0f, 10.0f, H2_MAX_PPM));
  // A reading above the ceiling is reported as the ceiling.
  EXPECT_FLOAT_EQ(H2_MAX_PPM, clamp_ppm(20000.0f, 0.0f, H2_MAX_PPM));
  EXPECT_FLOAT_EQ(500.0f, clamp_ppm(976.97f, 0.0f, 500.0f));
}

TEST(MqMath, CalibrationSamplesMustAgree) {
  // Clean air at 10 kOhm with ADC noise: a settled reference.
  EXPECT_TRUE(calibration_is_stable(9.8f, 10.2f, 10.0f));
  // A sensor that is still settling or not in clean air: (19 - 1) / 10 = 1.8.
  EXPECT_FALSE(calibration_is_stable(1.0f, 19.0f, 10.0f));
  // The limit is inclusive: 0.25 of the mean passes, a little more does not.
  EXPECT_TRUE(calibration_is_stable(9.0f, 11.0f, 8.0f));
  EXPECT_FALSE(calibration_is_stable(8.9f, 11.0f, 8.0f));
  // Unusable values are never a settled reference.
  EXPECT_FALSE(calibration_is_stable(NAN, 10.0f, 10.0f));
  EXPECT_FALSE(calibration_is_stable(10.0f, NAN, 10.0f));
  EXPECT_FALSE(calibration_is_stable(10.0f, 10.0f, NAN));
  EXPECT_FALSE(calibration_is_stable(10.0f, 10.0f, 0.0f));
}

TEST(MqMath, CorrectionFollowsTemperatureAndHumidity) {
  // At RH 33 % and 20 C the MQ-8 model reports 0.9052.
  EXPECT_NEAR(0.9052f, correction_coefficient(33.0f, 20.0f, MQ8_TC), 0.0001f);
  // The inputs are clamped to the fitted domain, and a missing reading disables
  // the correction instead of invalidating the measurement.
  EXPECT_FLOAT_EQ(1.0f, correction_coefficient(NAN, 20.0f, MQ8_TC));
  EXPECT_FLOAT_EQ(1.0f, correction_coefficient(33.0f, NAN, MQ8_TC));
  EXPECT_NEAR(correction_coefficient(33.0f, 20.0f, MQ8_TC), correction_coefficient(0.0f, 20.0f, MQ8_TC), 0.0001f);

  // The ratio is divided by the correction; 1.0 leaves it alone.
  EXPECT_FLOAT_EQ(1.0f, apply_correction(2.0f, 2.0f));
  EXPECT_FLOAT_EQ(2.0f, apply_correction(2.0f, 1.0f));

  // Absolute clamps to `max_ppm`, scaled to `max_ppm * correction`.
  EXPECT_FLOAT_EQ(1000.0f, clamp_ppm_corrected(3000.0f, 0.0f, 1000.0f, 2.0f, CLAMP_ABSOLUTE));
  EXPECT_FLOAT_EQ(2000.0f, clamp_ppm_corrected(3000.0f, 0.0f, 1000.0f, 2.0f, CLAMP_SCALED));
}

TEST(MqMath, InverseCorrectionMovesTheRatioTheOtherWay) {
  // RS/R0 and R0/RS run in opposite directions: the same correction has to be
  // divided into one and multiplied into the other.
  EXPECT_FLOAT_EQ(0.625f, apply_correction(0.5f, 0.8f));
  EXPECT_FLOAT_EQ(0.4f, apply_correction_inverse(0.5f, 0.8f));
  // A missing or unusable correction leaves the ratio alone.
  EXPECT_FLOAT_EQ(0.5f, apply_correction_inverse(0.5f, 1.0f));
  EXPECT_FLOAT_EQ(0.5f, apply_correction_inverse(0.5f, 0.0f));
  EXPECT_FLOAT_EQ(0.5f, apply_correction_inverse(0.5f, NAN));

  // Under R0/RS the inverse path has to stay closer to the uncorrected reading
  // than the RS/R0 path would: the correction is a small physical adjustment, not
  // a move to the other end of the curve.
  const float ratio = 1.5f;
  const float uncorrected = ppm_from_ratio(H2_A, H2_B, ratio, REGRESSION_EXPONENTIAL);
  const float wrong = ppm_from_ratio(H2_A, H2_B, apply_correction(ratio, 0.8f), REGRESSION_EXPONENTIAL);
  const float right = ppm_from_ratio(H2_A, H2_B, apply_correction_inverse(ratio, 0.8f), REGRESSION_EXPONENTIAL);
  EXPECT_TRUE(std::fabs(right - uncorrected) < std::fabs(wrong - uncorrected));
}

TEST(MqMath, CoefficientClampLimits) {
  EXPECT_FLOAT_EQ(static_cast<float>(mqmath::MQ_MAX_A), clamp_a(2e31f));
  EXPECT_FLOAT_EQ(-static_cast<float>(mqmath::MQ_MAX_A), clamp_a(-2e31f));
  EXPECT_FLOAT_EQ(static_cast<float>(mqmath::MQ_MAX_B), clamp_b(1000.0f));
  EXPECT_FLOAT_EQ(-static_cast<float>(mqmath::MQ_MAX_B), clamp_b(-1000.0f));
  // A non-finite coefficient would drop the curve, so it reads as no curve.
  EXPECT_FLOAT_EQ(0.0f, clamp_a(NAN));
  EXPECT_FLOAT_EQ(0.0f, clamp_b(INFINITY));
}

}  // namespace esphome::mq_gas_sensors::testing
