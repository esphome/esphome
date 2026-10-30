#include <array>
#include <utility>
#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_climate.h"

namespace esphome::mitsubishi_cn105::testing {

struct MitsubishiCN105ClimateTestContext {
  MockUARTComponent uart;
  MitsubishiCN105Component component;
  MitsubishiCN105Climate sut;

  MitsubishiCN105ClimateTestContext() {
    this->component.set_uart_parent(&this->uart);
    this->sut.set_parent(&this->component);
  }
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
  EXPECT_TRUE(traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE));
  EXPECT_TRUE(traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION));
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

TEST(MitsubishiCN105ClimateTests, TelemetryDisabledDisablesCurrentTemperatureAndActionTraits) {
  MitsubishiCN105ClimateTestContext context;
  context.component.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);

  const auto traits = context.sut.traits();
  EXPECT_FALSE(traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE));
  EXPECT_FALSE(traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION));
}

TEST(MitsubishiCN105ClimateTests, ActionMapsModeAndOperatingState) {
  using Mode = MitsubishiCN105::Mode;
  MitsubishiCN105ClimateTestContext context;
  context.sut.setup();

  // Room temperature 21.0°C and operating = true
  context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x07, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00, 0xAA, 0xAE});
  context.component.loop();
  context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x01, 0x61});
  context.component.loop();

  context.component.set_target_temperature(24.0f);

  // Power off -> CLIMATE_ACTION_OFF regardless of operating flag
  context.component.set_power(false);
  context.component.set_mode(Mode::HEAT);
  context.component.publish_status();
  EXPECT_EQ(context.sut.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_OFF);

  context.component.set_power(true);
  static constexpr std::array OPERATING_ACTIONS{
      std::pair{Mode::HEAT, climate::CLIMATE_ACTION_HEATING},
      std::pair{Mode::COOL, climate::CLIMATE_ACTION_COOLING},
      std::pair{Mode::DRY, climate::CLIMATE_ACTION_DRYING},
      std::pair{Mode::FAN_ONLY, climate::CLIMATE_ACTION_FAN},
  };
  for (const auto &[mode, action] : OPERATING_ACTIONS) {
    context.component.set_mode(mode);
    context.component.publish_status();
    EXPECT_EQ(context.sut.action, action) << "operating, mode " << static_cast<int>(mode);
  }

  // Switching to AUTO infers the direction from room (21°C) vs. target (24°C).
  context.component.set_mode(Mode::AUTO);
  context.component.publish_status();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_HEATING);

  // Overshooting the target while still operating keeps HEATING.
  context.component.set_target_temperature(18.0f);
  context.component.publish_status();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_HEATING);

  // Going idle and starting again with room > target reports COOLING.
  context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x62});
  context.component.loop();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_IDLE);
  context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x01, 0x61});
  context.component.loop();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_COOLING);

  // A tie on start reports IDLE until a direction is known.
  context.component.set_mode(Mode::HEAT);
  context.component.publish_status();
  context.component.set_target_temperature(21.0f);
  context.component.set_mode(Mode::AUTO);
  context.component.publish_status();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_IDLE);

  // Operating = false
  context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x62});
  context.component.loop();
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_IDLE);

  context.component.set_target_temperature(24.0f);
  static constexpr std::array NOT_OPERATING_ACTIONS{
      std::pair{Mode::HEAT, climate::CLIMATE_ACTION_IDLE},    std::pair{Mode::COOL, climate::CLIMATE_ACTION_IDLE},
      std::pair{Mode::DRY, climate::CLIMATE_ACTION_IDLE},     std::pair{Mode::AUTO, climate::CLIMATE_ACTION_IDLE},
      std::pair{Mode::FAN_ONLY, climate::CLIMATE_ACTION_FAN},
  };
  for (const auto &[mode, action] : NOT_OPERATING_ACTIONS) {
    context.component.set_mode(mode);
    context.component.publish_status();
    EXPECT_EQ(context.sut.action, action) << "not operating, mode " << static_cast<int>(mode);
  }
}

TEST(MitsubishiCN105ClimateTests, UnitWithoutOperatingStatusKeepsActionTraitAndReportsIdle) {
  MitsubishiCN105ClimateTestContext context;
  context.component.set_telemetry_request_min_interval(0);
  EXPECT_TRUE(context.sut.traits().has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION));
  context.sut.setup();
  context.component.setup();

  // Fake unit: powered on in AUTO at 24°C with a 21°C room, never answers 0x06.
  int operating_status_requests = 0;
  int room_temperature_requests = 0;
  for (uint32_t now = 0; now < 20000; now += 10) {
    TestableMitsubishiCN105::test_loop_time_ms = now;
    if (context.uart.tx.size() > 5) {
      const uint8_t type = context.uart.tx[1];
      const uint8_t msg_type = context.uart.tx[5];
      context.uart.tx.clear();
      if (type == 0x5A) {
        context.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});
      } else if (msg_type == 0x02) {
        context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x01, 0x08, 0x07,
                              0x00, 0x04, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B});
      } else if (msg_type == 0x03) {
        ++room_temperature_requests;
        context.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00,
                              0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5});
      } else if (msg_type == 0x06) {
        ++operating_status_requests;
      }
    }
    context.component.loop();
  }

  EXPECT_EQ(operating_status_requests, 3);
  EXPECT_GT(room_temperature_requests, 3);
  EXPECT_EQ(context.sut.mode, climate::CLIMATE_MODE_HEAT_COOL);
  EXPECT_FLOAT_EQ(context.sut.current_temperature, 21.0f);
  EXPECT_EQ(context.sut.action, climate::CLIMATE_ACTION_IDLE);
  EXPECT_TRUE(context.sut.traits().has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION));
}

}  // namespace esphome::mitsubishi_cn105::testing
