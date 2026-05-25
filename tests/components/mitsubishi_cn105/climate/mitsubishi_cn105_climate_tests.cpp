#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeOffLeavesTraitsEmpty) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_OFF);

  EXPECT_FALSE(sut.traits().get_supports_swing_modes());
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeVerticalExposesOffAndVertical) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeHorizontalExposesOffAndHorizontal) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, SupportedSwingModeBothExposesAllExpectedModes) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_TRUE(sut.traits().supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesMapsVerticalSwingWhenSupported) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::SWING;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::CENTER;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesMapsHorizontalSwingWhenSupported) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::AUTO;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::SWING;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_HORIZONTAL);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesMapsBothSwingWhenSupported) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::SWING;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::SWING;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_BOTH);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesMapsSwingOffWhenNoSwingActive) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::POSITION_3;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::CENTER;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesRemembersLastNonSwingPositions) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::POSITION_4;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::RIGHT;

  sut.apply_values_();

  EXPECT_EQ(sut.last_non_swing_vane_mode_, MitsubishiCN105::VaneMode::POSITION_4);
  EXPECT_EQ(sut.last_non_swing_wide_vane_mode_, MitsubishiCN105::WideVaneMode::RIGHT);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::SWING;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::SWING;

  sut.apply_values_();

  EXPECT_EQ(sut.last_non_swing_vane_mode_, MitsubishiCN105::VaneMode::POSITION_4);
  EXPECT_EQ(sut.last_non_swing_wide_vane_mode_, MitsubishiCN105::WideVaneMode::RIGHT);
  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_BOTH);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesDoesNotOverwriteRememberedPositionWithUnknownValues) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  sut.last_non_swing_vane_mode_ = MitsubishiCN105::VaneMode::POSITION_2;
  sut.last_non_swing_wide_vane_mode_ = MitsubishiCN105::WideVaneMode::LEFT;

  sut.status().vane_mode = MitsubishiCN105::VaneMode::UNKNOWN;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::UNKNOWN;

  sut.apply_values_();

  EXPECT_EQ(sut.last_non_swing_vane_mode_, MitsubishiCN105::VaneMode::POSITION_2);
  EXPECT_EQ(sut.last_non_swing_wide_vane_mode_, MitsubishiCN105::WideVaneMode::LEFT);
  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesIgnoresUnsupportedVerticalSwingState) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::SWING;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::CENTER;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(MitsubishiCN105ClimateTests, ApplyValuesIgnoresUnsupportedHorizontalSwingState) {
  TestableMitsubishiCN105Climate sut;

  sut.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  sut.status().vane_mode = MitsubishiCN105::VaneMode::AUTO;
  sut.status().wide_vane_mode = MitsubishiCN105::WideVaneMode::SWING;

  sut.apply_values_();

  EXPECT_EQ(sut.swing_mode, climate::CLIMATE_SWING_OFF);
}

}  // namespace esphome::mitsubishi_cn105::testing
