#include "../common.h"

namespace esphome::mq_gas_sensors::testing {

TEST(MqMath, SensorResistanceFollowsTheDivider) {
  // RS = (VCC * RL) / V - RL: half the supply across a 10 kOhm load is 10 kOhm.
  EXPECT_FLOAT_EQ(10.0f, rs_from_voltage(2.5f, 5.0f, 10.0f));
  // A shorted sensor (V at VCC) reads 0 kOhm, an open one reads 0 instead of +inf.
  EXPECT_FLOAT_EQ(0.0f, rs_from_voltage(5.0f, 5.0f, 10.0f));
  EXPECT_FLOAT_EQ(0.0f, rs_from_voltage(0.0f, 5.0f, 10.0f));
  EXPECT_FLOAT_EQ(0.0f, rs_from_voltage(NAN, 5.0f, 10.0f));
}

TEST(MqMath, RatioIsRsOverR0) {
  EXPECT_FLOAT_EQ(1.0f, ratio_from_rs(10.0f, 10.0f, 0.0f, RATIO_RS_R0));
  EXPECT_FLOAT_EQ(7.0f, ratio_from_rs(70.0f, 10.0f, 0.0f, RATIO_RS_R0));
  // Unusable inputs read 0 instead of a real ratio.
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(0.0f, 10.0f, 0.0f, RATIO_RS_R0));
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(10.0f, 0.0f, 0.0f, RATIO_RS_R0));
  EXPECT_FLOAT_EQ(0.0f, ratio_from_rs(NAN, 10.0f, 0.0f, RATIO_RS_R0));
}

TEST(MqMath, CleanAirReadingGivesR0) {
  // RS/R0 in clean air is 70 for the MQ-8: 700 kOhm of RS is 10 kOhm of R0.
  EXPECT_FLOAT_EQ(10.0f, r0_from_clean_air(700.0f, 70.0f, 0.0f));
  EXPECT_FLOAT_EQ(0.0f, r0_from_clean_air(0.0f, 70.0f, 0.0f));
  EXPECT_FLOAT_EQ(0.0f, r0_from_clean_air(700.0f, 0.0f, 0.0f));
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
  // No coefficients configured: 0 ppm, never a clamped range value.
  EXPECT_FLOAT_EQ(0.0f, ppm_from_ratio(0.0f, H2_B, 1.0f, REGRESSION_EXPONENTIAL));
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
