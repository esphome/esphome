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

TEST(MitsubishiClimateTest, FanModesFor3Levels) {
  MitsubishiClimate climate;
  climate.set_fan_mode(MITSUBISHI_FAN_3L);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_AUTO));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_LOW));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_MEDIUM));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_HIGH));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
}

TEST(MitsubishiClimateTest, FanModesFor4Levels) {
  MitsubishiClimate climate;
  climate.set_fan_mode(MITSUBISHI_FAN_4L);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
}

TEST(MitsubishiClimateTest, FanModesForQuietAnd4Levels) {
  MitsubishiClimate climate;
  climate.set_fan_mode(MITSUBISHI_FAN_Q4L);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
}

TEST(MitsubishiClimateTest, FanModesFollowTheLastSetFanMode) {
  MitsubishiClimate climate;
  climate.set_fan_mode(MITSUBISHI_FAN_Q4L);
  climate.set_fan_mode(MITSUBISHI_FAN_3L);
  auto traits = climate.get_traits();
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_HIGH));
}

}  // namespace esphome::mitsubishi::testing
