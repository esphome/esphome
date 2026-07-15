#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_climate.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeOffLeavesTraitsEmpty) {
  MitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_OFF);

  EXPECT_FALSE(sut.traits().get_supports_swing_modes());
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeVerticalExposesOffAndVertical) {
  MitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeHorizontalExposesOffAndHorizontal) {
  MitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeBothExposesAllExpectedModes) {
  MitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

}  // namespace esphome::mitsubishi_cn105::testing
