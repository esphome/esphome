#include <array>
#include <utility>
#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ClimateTests, CelsiusTemperatureMappingAndTraitsMatchExpectedValues) {
  TestableMitsubishiCN105Climate sut;
  const auto mapping = TemperatureMapping();

  for (int temperature = 16; temperature <= 31; ++temperature) {
    EXPECT_EQ(mapping.to_mitsubishi(temperature), temperature);
    EXPECT_EQ(mapping.from_mitsubishi(temperature), temperature);
  }

  const auto traits = sut.traits();
  EXPECT_EQ(traits.get_temperature_unit(), TemperatureUnit::CELSIUS);
  EXPECT_FLOAT_EQ(traits.get_visual_min_temperature(), 16.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_max_temperature(), 31.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_target_temperature_step(), 1.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_current_temperature_step(), 0.5f);
}

TEST(MitsubishiCN105ClimateTests, FahrenheitTemperatureMappingAndTraitsMatchExpectedValues) {
  TestableMitsubishiCN105Climate sut;
  auto mapping = TemperatureMapping();
  mapping.set_use_fahrenheit(true);
  sut.set_use_fahrenheit(true);

  const std::array cases{
      std::pair{61, 16.0f}, std::pair{62, 16.5f}, std::pair{63, 17.0f}, std::pair{64, 17.5f}, std::pair{65, 18.0f},
      std::pair{66, 18.5f}, std::pair{67, 19.0f}, std::pair{68, 20.0f}, std::pair{69, 21.0f}, std::pair{70, 21.5f},
      std::pair{71, 22.0f}, std::pair{72, 22.5f}, std::pair{73, 23.0f}, std::pair{74, 23.5f}, std::pair{75, 24.0f},
      std::pair{76, 24.5f}, std::pair{77, 25.0f}, std::pair{78, 25.5f}, std::pair{79, 26.0f}, std::pair{80, 26.5f},
      std::pair{81, 27.0f}, std::pair{82, 27.5f}, std::pair{83, 28.0f}, std::pair{84, 28.5f}, std::pair{85, 29.0f},
      std::pair{86, 29.5f}, std::pair{87, 30.0f}, std::pair{88, 30.5f},
  };

  for (const auto &[fahrenheit, mitsubishi_celsius] : cases) {
    EXPECT_FLOAT_EQ(mapping.to_mitsubishi(fahrenheit), mitsubishi_celsius);
    EXPECT_FLOAT_EQ(mapping.from_mitsubishi(mitsubishi_celsius), fahrenheit);
  }
  const auto traits = sut.traits();
  EXPECT_EQ(traits.get_temperature_unit(), TemperatureUnit::FAHRENHEIT);
  EXPECT_FLOAT_EQ(traits.get_visual_min_temperature(), 61.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_max_temperature(), 88.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_target_temperature_step(), 1.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_current_temperature_step(), 1.0f);
}

TEST(MitsubishiCN105ClimateTests, FahrenheitTemperatureMappingUsesLinearConversionOutsideSetpointRange) {
  auto mapping = TemperatureMapping();
  mapping.set_use_fahrenheit(true);

  const std::array cases{
      std::pair{0.0f, 32.0f},  std::pair{10.0f, 50.0f}, std::pair{15.5f, 59.9f},
      std::pair{31.0f, 87.8f}, std::pair{35.0f, 95.0f}, std::pair{40.0f, 104.0f},
  };

  for (const auto &[celsius, fahrenheit] : cases) {
    EXPECT_FLOAT_EQ(mapping.from_mitsubishi(celsius), fahrenheit);
  }
}

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
