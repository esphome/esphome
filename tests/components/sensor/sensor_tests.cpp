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

TEST(DeltaFilter, AbsoluteMinIsExclusiveAndMaxInclusive) {
  EXPECT_FALSE(delta_passes(12.0f, 10.0f, 2.0f, 10.0f));
  EXPECT_TRUE(delta_passes(20.0f, 10.0f, 2.0f, 10.0f));
  EXPECT_FALSE(delta_passes(21.0f, 10.0f, 2.0f, 10.0f));
  EXPECT_FALSE(delta_passes(12.0f, 10.0f, 2.0f));
  EXPECT_TRUE(delta_passes(-1e30f, 10.0f, 2.0f));
  EXPECT_TRUE(delta_passes(INFINITY, 10.0f, 2.0f));
}

TEST(DeltaFilter, PercentageBoundsScaleWithTheReference) {
  EXPECT_FALSE(delta_passes_pct(150.0f, 100.0f, 0.0f, 0.5f, INFINITY, 0.0f));
  EXPECT_TRUE(delta_passes_pct(151.0f, 100.0f, 0.0f, 0.5f, INFINITY, 0.0f));
  EXPECT_TRUE(delta_passes_pct(75.0f, 150.0f, 0.0f, 0.0f, 0.0f, 0.5f));
  EXPECT_FALSE(delta_passes_pct(74.0f, 150.0f, 0.0f, 0.0f, 0.0f, 0.5f));
}

TEST(DeltaFilter, InfiniteOrNaNReferenceNeverPasses) {
  EXPECT_FALSE(delta_passes(5.0f, INFINITY, 1.0f));
  EXPECT_FALSE(delta_passes(5.0f, -INFINITY, 0.0f));
  EXPECT_FALSE(delta_passes(5.0f, NAN, 0.0f));
  EXPECT_FALSE(delta_passes(5.0f, INFINITY, 1.0f, 10.0f));
  EXPECT_FALSE(delta_passes(5.0f, NAN, 0.0f, 10.0f));
  EXPECT_FALSE(delta_passes_pct(5.0f, INFINITY, 1.0f, 0.0f, 0.0f, 0.5f));
  EXPECT_FALSE(delta_passes_pct(5.0f, -INFINITY, 0.0f, 0.05f, INFINITY, 0.0f));
  EXPECT_FALSE(delta_passes_pct(5.0f, NAN, 0.0f, 0.05f, INFINITY, 0.0f));
}

TEST(DeltaFilter, PassesFirstAndNaNValuesAndKeepsTheLastPassedOne) {
  DeltaFilter filter([](float x, float value) { return delta_passes(value, x, 1.0f); });
  EXPECT_FALSE(filter.new_value(NAN).has_value());
  EXPECT_EQ(filter.new_value(10.0f).value_or(NAN), 10.0f);
  EXPECT_FALSE(filter.new_value(10.5f).has_value());
  EXPECT_EQ(filter.new_value(11.5f).value_or(NAN), 11.5f);
  EXPECT_TRUE(std::isnan(filter.new_value(NAN).value_or(0.0f)));
  EXPECT_FALSE(filter.new_value(NAN).has_value());
  EXPECT_EQ(filter.new_value(11.6f).value_or(NAN), 11.6f);
}

}  // namespace esphome::sensor::testing
