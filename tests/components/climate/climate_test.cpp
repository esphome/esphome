#include <gtest/gtest.h>
#include "esphome/components/climate/climate.h"

namespace esphome::climate::testing {

// Minimal concrete Climate that offers a fixed set of modes, so the restore path can be exercised
// without any hardware or platform component.
class TestClimate : public Climate {
 public:
  ClimateTraits traits() override {
    auto traits = ClimateTraits();
    traits.set_supported_modes({CLIMATE_MODE_OFF, CLIMATE_MODE_COOL});
    traits.set_supported_fan_modes({CLIMATE_FAN_LOW, CLIMATE_FAN_HIGH});
    return traits;
  }

 protected:
  void control(const ClimateCall &call) override {}
};

TEST(ClimateRestoreStateTest, RestoresASupportedMode) {
  TestClimate climate;
  // Value-initialized: several members (mode, swing_mode, the temperature union) have no default
  // member initializer, so leaving the {} off would read indeterminate values.
  ClimateDeviceRestoreState state{};
  state.mode = CLIMATE_MODE_COOL;

  state.apply(&climate);

  EXPECT_EQ(climate.mode, CLIMATE_MODE_COOL);
}

TEST(ClimateRestoreStateTest, DoesNotRestoreAnUnsupportedMode) {
  TestClimate climate;
  ClimateDeviceRestoreState state{};
  state.mode = CLIMATE_MODE_HEAT;

  state.apply(&climate);

  // The device never advertised HEAT, so the mode stays where it was.
  EXPECT_EQ(climate.mode, CLIMATE_MODE_OFF);
}

TEST(ClimateRestoreStateTest, LeavesTheCurrentModeAloneRatherThanForcingOff) {
  TestClimate climate;
  // apply() is public and nothing restricts it to setup(), so the entity is not necessarily off
  // when an unsupported mode is dropped. It keeps what it had rather than being forced to OFF.
  climate.mode = CLIMATE_MODE_COOL;
  ClimateDeviceRestoreState state{};
  state.mode = CLIMATE_MODE_HEAT;

  state.apply(&climate);

  EXPECT_EQ(climate.mode, CLIMATE_MODE_COOL);
}

TEST(ClimateRestoreStateTest, KeepsRestoringTheOtherFieldsWhenTheModeIsDropped) {
  TestClimate climate;
  ClimateDeviceRestoreState state{};
  state.mode = CLIMATE_MODE_HEAT;
  state.target_temperature = 21.0f;
  state.uses_custom_fan_mode = false;
  state.fan_mode = CLIMATE_FAN_HIGH;

  state.apply(&climate);

  EXPECT_EQ(climate.mode, CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(climate.target_temperature, 21.0f);
  // Compared as an optional: this asserts both that the fan mode was restored and what it holds.
  EXPECT_EQ(climate.fan_mode, CLIMATE_FAN_HIGH);
}

}  // namespace esphome::climate::testing
