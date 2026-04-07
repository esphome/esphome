#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ClimateTests, CelsiusTemperatureMappingMatchesExpectedValues) {
  const auto mapping = TemperatureMapping();

  for (int t = 16.0; t <= 31.0; t += 1.0) {
    EXPECT_EQ(mapping.to_mitsubishi(t), t);
  }

  for (int t = 16.0; t <= 31.0; t += 1.0) {
    EXPECT_EQ(mapping.from_mitsubishi(t), t);
  }
}

TEST(MitsubishiCN105ClimateTests, FahrenheitTemperatureMappingMatchesExpectedValues) {
  auto mapping = TemperatureMapping();
  mapping.set_fahrenheit(true);

  const std::array cases{
      std::pair{61, 16.0f}, std::pair{62, 16.5f}, std::pair{63, 17.0f}, std::pair{64, 17.5f}, std::pair{65, 18.0f},
      std::pair{66, 18.5f}, std::pair{67, 19.0f}, std::pair{68, 20.0f}, std::pair{69, 21.0f}, std::pair{70, 21.5f},
      std::pair{71, 22.0f}, std::pair{72, 22.5f}, std::pair{73, 23.0f}, std::pair{74, 23.5f}, std::pair{75, 24.0f},
      std::pair{76, 24.5f}, std::pair{77, 25.0f}, std::pair{78, 25.5f}, std::pair{79, 26.0f}, std::pair{80, 26.5f},
      std::pair{81, 27.0f}, std::pair{82, 27.5f}, std::pair{83, 28.0f}, std::pair{84, 28.5f}, std::pair{85, 29.0f},
      std::pair{86, 29.5f}, std::pair{87, 30.0f}, std::pair{88, 30.5f},
  };

  for (const auto &[fahrenheit, mitsubishi_celsius] : cases) {
    const float actual = mapping.to_mitsubishi((fahrenheit - 32.0f) / 1.8f);
    EXPECT_EQ(actual, mitsubishi_celsius);
  }

  for (const auto &[fahrenheit, mitsubishi_celsius] : cases) {
    const float ha_celsius = (fahrenheit - 32.0f) / 1.8f;
    const float actual = mapping.from_mitsubishi(mitsubishi_celsius);
    EXPECT_FLOAT_EQ(actual, ha_celsius);
  }
}

}  // namespace esphome::mitsubishi_cn105::testing
