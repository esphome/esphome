#include <gtest/gtest.h>
#include "esphome/components/mitsubishi/mitsubishi.h"

namespace esphome::mitsubishi::testing {

TEST(MitsubishiClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  MitsubishiClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

TEST(MitsubishiClimateTest, HeatCoolOverrideHiddenWithHeatAndCool) {
  MitsubishiClimate climate;
  climate.set_supports_heat(true);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  auto traits = climate.get_traits();
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_COOL));
}

}  // namespace esphome::mitsubishi::testing
