#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_climate.h"

namespace esphome::mitsubishi_cn105::testing {

struct MitsubishiCN105ClimateTestContext {
  MitsubishiCN105Component component;
  MitsubishiCN105Climate sut;

  MitsubishiCN105ClimateTestContext() { this->sut.set_parent(&this->component); }
};

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeOffLeavesTraitsEmpty) {
  MitsubishiCN105ClimateTestContext context;

  context.sut.set_supported_swing_mode(climate::CLIMATE_SWING_OFF);

  EXPECT_FALSE(context.sut.traits().get_supports_swing_modes());
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeVerticalExposesOffAndVertical) {
  MitsubishiCN105ClimateTestContext context;

  context.sut.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeHorizontalExposesOffAndHorizontal) {
  MitsubishiCN105ClimateTestContext context;

  context.sut.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_FALSE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeBothExposesAllExpectedModes) {
  MitsubishiCN105ClimateTestContext context;

  context.sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_TRUE(context.sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

}  // namespace esphome::mitsubishi_cn105::testing
