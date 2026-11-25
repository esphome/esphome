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
  EXPECT_FLOAT_EQ(1.0f, ratio_from_rs(10.0f, 10.0f));
  EXPECT_FLOAT_EQ(7.0f, ratio_from_rs(70.0f, 10.0f));
  // RS = 0 is a real reading (V_AO at VCC), the saturated end of the range.
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(0.0f, 10.0f));
  // Any other unusable input reads unknown, never a ratio the curve would turn
  // into an alarm reading at the top of the range.
  EXPECT_TRUE(std::isnan(ratio_from_rs(10.0f, 0.0f)));
  EXPECT_TRUE(std::isnan(ratio_from_rs(NAN, 10.0f)));
  EXPECT_TRUE(std::isnan(ratio_from_rs(-1.0f, 10.0f)));
}

TEST(MqMath, MidRangeReadingsFollowTheCurve) {
  // 976.97 * 1^-0.688 = 976.97 ppm.
  EXPECT_NEAR(976.97f, ppm_from_ratio(H2_A, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL), 0.01f);
  // 976.97 * 70^-0.688 = 52.5 ppm, the clean-air anchor of the MQ-8 H2 curve.
  EXPECT_NEAR(52.5f, ppm_from_ratio(H2_A, H2_B, 70.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL), 0.1f);
  // Linear: log10(ppm) = (log10(1) - 0.4491) / -0.47712 = 0.9413 -> 8.73 ppm.
  EXPECT_NEAR(8.73f, ppm_from_ratio(NH3_A, NH3_B, 1.0f, RegressionMethod::REGRESSION_METHOD_LINEAR), 0.01f);
}

TEST(MqMath, SaturatedSensorReportsTheTopOfTheRange) {
  // A ratio of 0 is RS at 0 (V_AO at VCC), the output of a saturated sensor:
  // the model reports the top of the range there, so it is clamped to max_ppm
  // instead of reading as clean air.
  EXPECT_EQ(FLT_MAX, ppm_from_ratio(H2_A, H2_B, 0.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL));
  EXPECT_FLOAT_EQ(
      H2_MAX_PPM,
      clamp_ppm(ppm_from_ratio(H2_A, H2_B, 0.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL), 0.0f, H2_MAX_PPM));
}

TEST(MqMath, ZeroRatioFollowsTheCurveDirection) {
  // A curve that falls as the ratio falls reaches 0 ppm at a ratio of 0: that is
  // the bottom of the range, not the saturated top the datasheet curve reports.
  EXPECT_FLOAT_EQ(0.0f, ppm_from_ratio(10.0f, 2.0f, 0.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_ratio(10.0f, 2.0f, 0.0f, RegressionMethod::REGRESSION_METHOD_INVERSE));
  // A coefficient pair the curve cannot evaluate stays unknown instead of picking
  // one end.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, 2.0f, 0.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(10.0f, 0.0f, 0.0f, RegressionMethod::REGRESSION_METHOD_INVERSE)));
  // A negative ratio is not a sensor resistance: unknown, never a concentration.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(10.0f, 2.0f, -0.5f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL)));
  // The MQ-131 ozone curve gives 0.12 ppm in clean air (ratio 15), far below the
  // 1 ppm top of its 10 - 1000 ppb range.
  EXPECT_NEAR(0.115f, ppm_from_ratio(0.41195f, -0.4708f, 15.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL),
              0.001f);
}

TEST(MqMath, UnusableInputReadsUnknown) {
  EXPECT_TRUE(std::isnan(ppm_from_ratio(H2_A, H2_B, NAN, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(clamp_ppm(NAN, 0.0f, H2_MAX_PPM)));
  // Coefficients the method cannot evaluate (log10 of a negative a) read
  // unknown, not a saturated max_ppm.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(-H2_A, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(
      clamp_ppm(ppm_from_ratio(-H2_A, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL), 0.0f, H2_MAX_PPM)));
  // No coefficients the method can use: a curve with a = 0 has no solution, so
  // it reads unknown rather than 0 ppm (clean air) or a clamped range value.
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_EXPONENTIAL)));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_LINEAR)));
  EXPECT_TRUE(std::isnan(ppm_from_ratio(0.0f, H2_B, 1.0f, RegressionMethod::REGRESSION_METHOD_INVERSE)));
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

}  // namespace esphome::mq_gas_sensors::testing
