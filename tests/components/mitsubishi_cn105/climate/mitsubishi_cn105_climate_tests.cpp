#include <array>
#include <utility>
#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_climate.h"

namespace esphome::mitsubishi_cn105::testing {

struct MitsubishiCN105ClimateTestContext {
  MitsubishiCN105Component component;
  MitsubishiCN105Climate sut;

  MitsubishiCN105ClimateTestContext() { this->sut.set_parent(&this->component); }
};

TEST(MitsubishiCN105ClimateTests, CelsiusTemperatureMappingAndTraitsMatchExpectedValues) {
  MitsubishiCN105ClimateTestContext context;

  for (int temperature = 16; temperature <= 31; ++temperature) {
    EXPECT_EQ(context.component.get_temperature_mapping().to_mitsubishi(temperature), temperature);
    EXPECT_EQ(context.component.get_temperature_mapping().from_mitsubishi(temperature), temperature);
  }

  const auto traits = context.sut.traits();
  EXPECT_EQ(traits.get_temperature_unit(), TemperatureUnit::CELSIUS);
  EXPECT_FLOAT_EQ(traits.get_visual_min_temperature(), 16.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_max_temperature(), 31.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_target_temperature_step(), 1.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_current_temperature_step(), 0.5f);
}

TEST(MitsubishiCN105ClimateTests, FahrenheitTemperatureMappingAndTraitsMatchExpectedValues) {
  MitsubishiCN105ClimateTestContext context;
  context.component.set_use_fahrenheit(true);

  const std::array cases{
      std::pair{61, 16.0f}, std::pair{62, 16.5f}, std::pair{63, 17.0f}, std::pair{64, 17.5f}, std::pair{65, 18.0f},
      std::pair{66, 18.5f}, std::pair{67, 19.0f}, std::pair{68, 20.0f}, std::pair{69, 21.0f}, std::pair{70, 21.5f},
      std::pair{71, 22.0f}, std::pair{72, 22.5f}, std::pair{73, 23.0f}, std::pair{74, 23.5f}, std::pair{75, 24.0f},
      std::pair{76, 24.5f}, std::pair{77, 25.0f}, std::pair{78, 25.5f}, std::pair{79, 26.0f}, std::pair{80, 26.5f},
      std::pair{81, 27.0f}, std::pair{82, 27.5f}, std::pair{83, 28.0f}, std::pair{84, 28.5f}, std::pair{85, 29.0f},
      std::pair{86, 29.5f}, std::pair{87, 30.0f}, std::pair{88, 30.5f},
  };

  for (const auto &[fahrenheit, mitsubishi_celsius] : cases) {
    EXPECT_FLOAT_EQ(context.component.get_temperature_mapping().to_mitsubishi(fahrenheit), mitsubishi_celsius);
    EXPECT_FLOAT_EQ(context.component.get_temperature_mapping().from_mitsubishi(mitsubishi_celsius), fahrenheit);
  }
  const auto traits = context.sut.traits();
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
