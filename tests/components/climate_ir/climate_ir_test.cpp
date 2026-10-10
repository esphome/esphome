#include <gtest/gtest.h>
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::climate_ir::testing {

class TestClimateIR : public ClimateIR {
 public:
  explicit TestClimateIR(bool supports_dry = false, bool supports_fan_only = false)
      : ClimateIR(16.0f, 30.0f, 1.0f, supports_dry, supports_fan_only) {}

  using ClimateIR::traits;

 protected:
  void transmit_state() override {}
};

// The HEAT_COOL default is covered in tests/component_tests/climate_ir.

TEST(ClimateIRTest, HeatCoolAdvertisedWhenSupported) {
  TestClimateIR climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  EXPECT_TRUE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, HeatCoolNotAdvertisedWhenUnsupported) {
  TestClimateIR climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, HeatCoolAdvertisedForCoolOnlyDeviceThatSupportsIt) {
  TestClimateIR climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

TEST(ClimateIRTest, HeatAndCoolModesFollowTheirOwnFlags) {
  TestClimateIR climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  auto traits = climate.traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_OFF));
}

TEST(ClimateIRTest, DefaultModes) {
  TestClimateIR climate;
  auto traits = climate.traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_OFF));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_COOL));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_DRY));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_FAN_ONLY));
}

TEST(ClimateIRTest, DryAndFanOnlyFromConstructor) {
  TestClimateIR climate(true, true);
  auto traits = climate.traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_DRY));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_FAN_ONLY));
}

TEST(ClimateIRTest, SetterCanTurnAModeBackOn) {
  TestClimateIR climate;
  climate.set_supports_cool(false);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_COOL));
  climate.set_supports_cool(true);
  EXPECT_TRUE(climate.traits().supports_mode(climate::CLIMATE_MODE_COOL));
}

}  // namespace esphome::climate_ir::testing
