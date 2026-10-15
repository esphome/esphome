#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/gree/gree.h"

namespace esphome::gree::testing {

// Re-expose the protected frame helper for testing, as tests/components/fusb302b/pd_test.cpp does.
class TestableGreeClimate : public GreeClimate {
 public:
  using GreeClimate::fahrenheit_fields_;
};

TEST(GreeClimateTest, HeatCoolHiddenWithoutHeatByDefault) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(GreeClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

// The unit displays trunc(celsiusToFahrenheit(GREE_TEMP_MIN + Temp + 0.5 * TempExtraDegreeF)).
// These expectations come from frames captured off a real YX1FF remote and from the panel of a
// Blueridge mini-split, not from a conversion formula.
TEST(GreeClimateTest, FahrenheitFieldsMatchCapturedRemoteFrames) {
  struct Expectation {
    float celsius;
    uint8_t temp_field;
    bool extra_degree;
  };
  // 18.33 C is how Home Assistant delivers a 65 F request, 26.67 C a 78 F request, and so on.
  const Expectation expectations[] = {
      {18.33f, 2, true},    // 65 F -- captured remote frame b1=02 b3=5C
      {18.89f, 3, false},   // 66 F -- captured remote frame b1=03 b3=58
      {21.67f, 6, false},   // 71 F
      {22.22f, 6, true},    // 72 F
      {25.0f, 9, false},    // 77 F
      {25.56f, 10, false},  // 78 F
  };
  TestableGreeClimate climate;
  for (const auto &expectation : expectations) {
    uint8_t temp_field = 0xFF;
    bool extra_degree = false;
    climate.fahrenheit_fields_(expectation.celsius, temp_field, extra_degree);
    EXPECT_EQ(expectation.temp_field, temp_field) << "celsius " << expectation.celsius;
    EXPECT_EQ(expectation.extra_degree, extra_degree) << "celsius " << expectation.celsius;
  }
}

TEST(GreeClimateTest, FahrenheitFieldsCoverEveryWholeDegreeInRange) {
  // 61..86 F is the range a YX1FF remote offers; each should be exactly reachable.
  TestableGreeClimate climate;
  for (int fahrenheit = 61; fahrenheit <= 86; fahrenheit++) {
    const float celsius = (static_cast<float>(fahrenheit) - 32.0f) * 5.0f / 9.0f;
    uint8_t temp_field = 0xFF;
    bool extra_degree = false;
    climate.fahrenheit_fields_(celsius, temp_field, extra_degree);
    const float encoded = static_cast<float>(GREE_TEMP_MIN + temp_field) + (extra_degree ? 0.5f : 0.0f);
    EXPECT_EQ(fahrenheit, static_cast<int>(encoded * 9.0f / 5.0f + 32.0f)) << "fahrenheit " << fahrenheit;
  }
}

TEST(GreeClimateTest, FahrenheitFieldsHandleNonFiniteTarget) {
  // transmit_state() can run from setup() via restore_state_(), when the target may still be NaN.
  TestableGreeClimate climate;
  uint8_t temp_field = 0xFF;
  bool extra_degree = true;
  climate.fahrenheit_fields_(NAN, temp_field, extra_degree);
  EXPECT_EQ(0, temp_field);
  EXPECT_FALSE(extra_degree);
}

}  // namespace esphome::gree::testing
