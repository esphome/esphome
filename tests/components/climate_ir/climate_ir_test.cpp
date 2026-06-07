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

TEST(ClimateIRTest, HeatCoolAdvertisedWhenHeatAndCoolSupported) {
  TestClimateIR climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(true);
  EXPECT_TRUE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, HeatCoolNotAdvertisedWhenOnlyHeatSupported) {
  TestClimateIR climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(false);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, HeatCoolNotAdvertisedWhenOnlyCoolSupported) {
  TestClimateIR climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, HeatCoolNotAdvertisedWhenNeitherSupported) {
  TestClimateIR climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(false);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, SupportsAutoOverrideForcesHeatCoolOnForCoolOnlyDevice) {
  TestClimateIR climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_auto(true);
  EXPECT_TRUE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(ClimateIRTest, SupportsAutoOverrideForcesHeatCoolOffForHeatAndCoolDevice) {
  TestClimateIR climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(true);
  climate.set_supports_auto(false);
  EXPECT_FALSE(climate.traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

}  // namespace esphome::climate_ir::testing
