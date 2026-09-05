#include <gtest/gtest.h>
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::climate_ir::testing {

// Minimal concrete ClimateIR for testing traits() without IR transmission hardware.
class TestClimateIR : public ClimateIR {
 public:
  TestClimateIR() : ClimateIR(16.0f, 30.0f) {}

  using ClimateIR::traits;

 protected:
  void transmit_state() override {}
};

// Whether HEAT_COOL is supported by default is decided during code generation (see
// tests/component_tests/climate_ir). What traits() owns is honoring the resolved flag, including
// for devices whose HEAT_COOL support doesn't match their heat/cool capabilities.

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

}  // namespace esphome::climate_ir::testing
