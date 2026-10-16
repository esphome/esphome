#include "../common.h"

namespace esphome::mics_5524::testing {

TEST(MicsMath, SaturatedSensorReportsTheTopOfTheRange) {
  // ratio 0 is V_AO at VCC.  The vendor helpers return the top of the range there,
  // so a saturated sensor must not be converted into a clean-air reading.
  EXPECT_FLOAT_EQ(CO.max_ppm, ppm_from_vendor_curve(0.0f, CO));
  EXPECT_FLOAT_EQ(H2.max_ppm, ppm_from_vendor_curve(0.0f, H2));
  EXPECT_FLOAT_EQ(NH3.max_ppm, ppm_from_vendor_curve(0.0f, NH3));
  EXPECT_FLOAT_EQ(C2H5OH.max_ppm, ppm_from_vendor_curve(0.0f, C2H5OH));
  EXPECT_FLOAT_EQ(CH4.max_ppm, ppm_from_vendor_curve(0.0f, CH4));
}

TEST(MicsMath, GasFreeAirReadsZero) {
  // At or above the threshold the vendor reports no gas, and so does a value below
  // its own measuring range.
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(CO.threshold, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(CO.threshold + 0.01f, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(1.0f, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(0.4249f, CO));  // 0.25 ppm, below the range
}

TEST(MicsMath, MidRangeReadingsFollowTheVendorFormula) {
  // (0.425 - 0.2) / 0.000405 = 555.56 ppm
  EXPECT_NEAR(555.556f, ppm_from_vendor_curve(0.2f, CO), 0.01f);
  // (0.8 - 0.7) / 0.0015 = 66.67 ppm
  EXPECT_NEAR(66.667f, ppm_from_vendor_curve(0.7f, NH3), 0.01f);
  // (0.279 - 0.1) / 0.00026 = 688.46 ppm
  EXPECT_NEAR(688.462f, ppm_from_vendor_curve(0.1f, H2), 0.01f);
}

TEST(MicsMath, UnusableInputReadsUnknown) {
  EXPECT_TRUE(std::isnan(ppm_from_vendor_curve(NAN, CO)));
  EXPECT_TRUE(std::isnan(ppm_from_vendor_curve(INFINITY, CO)));
  EXPECT_TRUE(std::isnan(air_value_from_voltage(NAN, 5.0f)));
  EXPECT_TRUE(std::isnan(air_value_from_voltage(4.0f, NAN)));
  EXPECT_TRUE(std::isnan(ratio_from_air_value(1.0f, NAN)));
  EXPECT_TRUE(std::isnan(ratio_from_air_value(NAN, 1.0f)));
  // A missing clean-air reference (x_air = 0) must not be read as a real ratio.
  EXPECT_TRUE(std::isnan(ratio_from_air_value(1.0f, 0.0f)));
}

TEST(MicsMath, AirValueIsTheVoltageBelowVcc) {
  EXPECT_FLOAT_EQ(1.0f, air_value_from_voltage(4.0f, 5.0f));
  // At VCC the vendor value is 0, the saturated end of the range ...
  EXPECT_FLOAT_EQ(0.0f, air_value_from_voltage(5.0f, 5.0f));
  // ... but a voltage above VCC cannot come from the sensor, so it is unusable rather
  // than saturated: a wrong `vcc:` is not reported as a real reading.
  EXPECT_TRUE(std::isnan(air_value_from_voltage(5.2f, 5.0f)));
  // The two ends of the vendor range, and the unusable case, one by one.
  EXPECT_FLOAT_EQ(1.0f, ratio_from_air_value(air_value_from_voltage(4.0f, 5.0f), 1.0f));
  EXPECT_FLOAT_EQ(0.0f, ratio_from_air_value(air_value_from_voltage(5.0f, 5.0f), 1.0f));
  EXPECT_TRUE(std::isnan(ratio_from_air_value(air_value_from_voltage(6.0f, 5.0f), 1.0f)));
}

TEST(MicsMath, CalibrationSamplesMustAgree) {
  // Clean air at 3.6 V with a few millivolts of ADC noise: a settled reference.
  EXPECT_TRUE(calibration_is_stable(3.555f, 3.645f, 3.6f));
  // A sensor that is still settling or not in clean air: (4.9 - 0.1) / 2.5 = 1.92.
  EXPECT_FALSE(calibration_is_stable(0.1f, 4.9f, 2.5f));
  // The limit is inclusive: 0.25 of the mean passes, a little more does not.
  EXPECT_TRUE(calibration_is_stable(3.0f, 4.0f, 4.0f));
  EXPECT_FALSE(calibration_is_stable(2.9f, 4.0f, 4.0f));
  // Unusable values are never a settled reference.
  EXPECT_FALSE(calibration_is_stable(NAN, 3.6f, 3.6f));
  EXPECT_FALSE(calibration_is_stable(3.6f, NAN, 3.6f));
  EXPECT_FALSE(calibration_is_stable(3.6f, 3.6f, NAN));
  EXPECT_FALSE(calibration_is_stable(3.6f, 3.6f, 0.0f));
}

TEST(MicsMath, ConfiguredCeilingCapsTheReading) {
  // `max_ppm:` narrows the vendor range: a reading above the ceiling is reported as
  // the ceiling, never as a value the sensor is not configured for.
  const micsmath::VendorCurve narrow{0.425f, 0.000405f, 1.0f, 500.0f};
  EXPECT_FLOAT_EQ(500.0f, ppm_from_vendor_curve(0.2f, narrow));  // 555.56 ppm
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(0.5f, narrow));    // above the threshold
}

TEST(MicsMath, SaturatedSensorEndToEnd) {
  // V_AO at VCC -> x = 0 -> ratio 0 -> top of the range, the case that used to read
  // 0 ppm and made a saturated sensor look like clean air.
  const float x = air_value_from_voltage(5.0f, 5.0f);
  const float ratio = ratio_from_air_value(x, 1.0f);

  EXPECT_FLOAT_EQ(0.0f, ratio);
  EXPECT_FLOAT_EQ(CO.max_ppm, ppm_from_vendor_curve(ratio, CO));
}

}  // namespace esphome::mics_5524::testing
