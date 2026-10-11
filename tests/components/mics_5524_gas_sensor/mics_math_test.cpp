#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/mics_5524_gas_sensor/mics_math.h"

namespace esphome::mics_5524_gas_sensor::testing {

using micsmath::air_value_from_voltage;
using micsmath::clamp_ppm;
using micsmath::ppm_from_vendor_curve;
using micsmath::ratio_from_air_value;

// The table of `coefficients.py`, written out so a change there shows up here.
static constexpr micsmath::VendorCurve CO{0.425f, 0.000405f, 1.0f, 1000.0f};
static constexpr micsmath::VendorCurve H2{0.279f, 0.00026f, 1.0f, 1000.0f};
static constexpr micsmath::VendorCurve NH3{0.8f, 0.0015f, 10.0f, 500.0f};
static constexpr micsmath::VendorCurve C2H5OH{0.306f, 0.00057f, 10.0f, 500.0f};
static constexpr micsmath::VendorCurve CH4{0.786f, 0.000023f, 1000.0f, 25000.0f};

TEST(MicsMath, SaturatedSensorReportsTheTopOfTheRange) {
  // ratio 0 is V_AO at VCC.  The vendor helpers return the top of the range there,
  // so a saturated sensor must not be converted into a clean-air reading.
  EXPECT_FLOAT_EQ(CO.max_ppm, ppm_from_vendor_curve(0.0f, CO));
  EXPECT_FLOAT_EQ(H2.max_ppm, ppm_from_vendor_curve(0.0f, H2));
  EXPECT_FLOAT_EQ(NH3.max_ppm, ppm_from_vendor_curve(0.0f, NH3));
  EXPECT_FLOAT_EQ(C2H5OH.max_ppm, ppm_from_vendor_curve(0.0f, C2H5OH));
  EXPECT_FLOAT_EQ(CH4.max_ppm, ppm_from_vendor_curve(0.0f, CH4));
}

TEST(MicsMath, GasFreeAirReadsZeroNotTheMinimum) {
  // At or above the threshold the vendor reports no gas, and so does a value below
  // its own measuring range.  Neither may be lifted to `min_ppm` by the caller.
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(CO.threshold, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(CO.threshold + 0.01f, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(1.0f, CO));
  EXPECT_FLOAT_EQ(0.0f, ppm_from_vendor_curve(0.4249f, CO));  // 0.25 ppm, below the range
  EXPECT_FLOAT_EQ(0.0f, clamp_ppm(ppm_from_vendor_curve(CO.threshold, CO), 1.0f, 1000.0f));
  EXPECT_FLOAT_EQ(0.0f, clamp_ppm(0.0f, 5.0f, 1000.0f));
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
  EXPECT_TRUE(std::isnan(clamp_ppm(NAN, 0.0f, 1000.0f)));
  EXPECT_TRUE(std::isnan(air_value_from_voltage(NAN, 5.0f)));
  EXPECT_TRUE(std::isnan(air_value_from_voltage(4.0f, NAN)));
  EXPECT_TRUE(std::isnan(ratio_from_air_value(1.0f, NAN)));
  EXPECT_TRUE(std::isnan(ratio_from_air_value(NAN, 1.0f)));
  // A missing clean-air reference (x_air = 0) must not be read as a real ratio.
  EXPECT_TRUE(std::isnan(ratio_from_air_value(1.0f, 0.0f)));
}

TEST(MicsMath, AirValueIsTheVoltageBelowVcc) {
  EXPECT_FLOAT_EQ(1.0f, air_value_from_voltage(4.0f, 5.0f));
  // At or above VCC the vendor value is 0 ...
  EXPECT_FLOAT_EQ(0.0f, air_value_from_voltage(5.0f, 5.0f));
  EXPECT_FLOAT_EQ(0.0f, air_value_from_voltage(5.2f, 5.0f));
  // ... which is the saturated end of the range, not "unknown".
  EXPECT_FLOAT_EQ(1.0f, ratio_from_air_value(air_value_from_voltage(4.0f, 5.0f), 1.0f));
  EXPECT_FLOAT_EQ(0.0f, ratio_from_air_value(air_value_from_voltage(5.0f, 5.0f), 1.0f));
}

TEST(MicsMath, ClampLiftsOnlyDetectedReadings) {
  EXPECT_FLOAT_EQ(1.0f, clamp_ppm(0.5f, 1.0f, 1000.0f));
  EXPECT_FLOAT_EQ(1000.0f, clamp_ppm(2000.0f, 1.0f, 1000.0f));
  EXPECT_FLOAT_EQ(500.0f, clamp_ppm(555.0f, 0.0f, 500.0f));
  EXPECT_FLOAT_EQ(10.0f, clamp_ppm(5.0f, 10.0f, 500.0f));
}

TEST(MicsMath, SaturatedSensorEndToEnd) {
  // V_AO at VCC -> x = 0 -> ratio 0 -> top of the range, the case that used to read
  // 0 ppm and made a saturated sensor look like clean air.
  const float x = air_value_from_voltage(5.0f, 5.0f);
  const float ratio = ratio_from_air_value(x, 1.0f);

  EXPECT_FLOAT_EQ(0.0f, ratio);
  EXPECT_FLOAT_EQ(1000.0f, clamp_ppm(ppm_from_vendor_curve(ratio, CO), 1.0f, 1000.0f));
}

}  // namespace esphome::mics_5524_gas_sensor::testing
