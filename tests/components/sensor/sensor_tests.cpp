#include <gtest/gtest.h>
#include <cmath>

#include "esphome/components/sensor/filter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

// 40 % read 7.3 °C warm at a true 25 °C, in single precision Magnus arithmetic
static constexpr float CORRECTED = 61.13f;

struct RhFixture {
  Sensor temperature;
  RHCorrectionFilter filter;

  RhFixture(float (*offset)(), bool use_fahrenheit = false) : filter(offset, &this->temperature, use_fahrenheit) {}
};

TEST(RHCorrectionFilter, CorrectsForTheOffset) {
  RhFixture f([]() -> float { return -7.3f; });
  f.temperature.publish_state(25.0f);
  EXPECT_NEAR(f.filter.new_value(40.0f).value_or(NAN), CORRECTED, 0.05f);
}

TEST(RHCorrectionFilter, PositiveOffsetLowersTheHumidity) {
  RhFixture f([]() -> float { return 7.3f; });
  f.temperature.publish_state(25.0f);
  EXPECT_LT(f.filter.new_value(40.0f).value_or(NAN), 40.0f);
}

TEST(RHCorrectionFilter, FahrenheitReadingsGiveTheSameResult) {
  // 77 °F is 25 °C and -13.14 °F is -7.3 °C of offset
  RhFixture f([]() -> float { return -13.14f; }, true);
  f.temperature.publish_state(77.0f);
  EXPECT_NEAR(f.filter.new_value(40.0f).value_or(NAN), CORRECTED, 0.05f);
}

TEST(RHCorrectionFilter, ClampsToOneHundredPercent) {
  RhFixture f([]() -> float { return -7.3f; });
  f.temperature.publish_state(25.0f);
  EXPECT_FLOAT_EQ(f.filter.new_value(90.0f).value_or(NAN), 100.0f);
}

TEST(RHCorrectionFilter, PassesThroughWithoutAUsableTemperature) {
  RhFixture f([]() -> float { return -7.3f; });
  // No temperature reading yet
  EXPECT_FLOAT_EQ(f.filter.new_value(40.0f).value_or(NAN), 40.0f);
  // Out of the -40..50 °C band
  f.temperature.publish_state(60.0f);
  EXPECT_FLOAT_EQ(f.filter.new_value(40.0f).value_or(NAN), 40.0f);
  // The band also covers the reconstructed enclosure temperature
  f.temperature.publish_state(45.0f);
  EXPECT_FLOAT_EQ(f.filter.new_value(40.0f).value_or(NAN), 40.0f);
}

TEST(RHCorrectionFilter, DropsTheUpdateOnNaN) {
  RhFixture f([]() -> float { return -7.3f; });
  f.temperature.publish_state(NAN);
  EXPECT_FALSE(f.filter.new_value(40.0f).has_value());

  RhFixture nan_offset([]() -> float { return NAN; });
  nan_offset.temperature.publish_state(25.0f);
  EXPECT_FALSE(nan_offset.filter.new_value(40.0f).has_value());
}

TEST(ToNTCResistanceFilter, BConstantCalibration) {
  // b_constant 3950 K, 25 °C, 10 kOhm, as ntc_calc_b_constant() computes it
  ToNTCResistanceFilter filter(1 / 298.15 - 9.210340371976184 / 3950.0, 1 / 3950.0, 0);
  EXPECT_NEAR(filter.new_value(25.0f).value_or(NAN), 10000.0f, 0.5f);
  EXPECT_NEAR(filter.new_value(0.0f).value_or(NAN), 33620.6f, 1.0f);
}

}  // namespace esphome::sensor::testing
