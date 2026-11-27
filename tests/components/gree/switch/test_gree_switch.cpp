#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::gree::testing {
namespace {

// Stands in for the hub pointer code generation gives each switch's apply function.
GreeClimate *hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<GreeFeature F> void apply_feature(switch_::Switch * /*entity*/, bool state) {
  hub->set_feature_state(F, state);
}

// The switch class code generation emits for `apply="set_feature_state(<feature>, state)"`.
template<GreeFeature F> using FeatureSwitch = switch_::ApplySwitch<apply_feature<F>>;

void initialize_climate(GreeClimate &device, Model model, CapturingTransmitter &transmitter) {
  hub = &device;
  device.set_model(model);
  device.set_transmitter(&transmitter);
  device.mode = climate::CLIMATE_MODE_COOL;
  device.target_temperature = 20;
  device.fan_mode = climate::CLIMATE_FAN_LOW;
  device.swing_mode = climate::CLIMATE_SWING_OFF;
  device.preset = climate::CLIMATE_PRESET_NONE;
}

/// Set up the climate as the application would at boot, with nothing saved.
void setup_climate(GreeClimate &device, Model model, CapturingTransmitter &transmitter) {
  if (global_preferences != nullptr)
    global_preferences->reset();
  static char name[] = "test_gree";
  static char friendly_name[] = "";
  App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  hub = &device;
  device.set_model(model);
  device.set_transmitter(&transmitter);
  static_cast<Component &>(device).setup();
}

GreeClimateData decode_transmission(const CapturingTransmitter &transmitter) {
  auto decoded = GreeClimateCodec::decode(GREE_YB1FA, transmitter.frame(GREE_YB1FA));
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(GreeClimateData{climate::CLIMATE_MODE_OFF, GREE_TEMP_MIN, climate::CLIMATE_FAN_AUTO,
                                          climate::CLIMATE_SWING_OFF, climate::CLIMATE_PRESET_NONE});
}

void receive_yb1fa_turbo(GreeClimate &device) {
  ASSERT_TRUE(receive(device, GREE_YB1FA, {0x49, 0x04, 0x70, 0x50, 0x01, 0x21, 0x00, 0x90}));
}

}  // namespace

// Code generation disables restoring for the models that receive feature state over IR.
TEST(GreeFeatureSwitch, DisabledRestoreSetupUsesParentStateWithoutTransmit) {
  for (const Model model : {GREE_YB1FA, GREE_YX1FF}) {
    CapturingTransmitter transmitter;
    GreeClimate device;
    FeatureSwitch<GREE_FEATURE_LIGHT> light;
    light.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    device.register_feature_switch(GREE_FEATURE_LIGHT, &light);

    setup_climate(device, model, transmitter);

    EXPECT_EQ(transmitter.send_count, 0U);
    EXPECT_TRUE(light.has_state());
    EXPECT_TRUE(light.state);
  }
}

TEST(GreeFeatureSwitch, LegacySetupRestoresAllSwitchesInOneTransmission) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  turbo.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);
  FeatureSwitch<GREE_FEATURE_LIGHT> light;
  light.set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF);
  device.register_feature_switch(GREE_FEATURE_LIGHT, &light);

  setup_climate(device, GREE_YAN, transmitter);

  EXPECT_EQ(transmitter.send_count, 1U);
  EXPECT_TRUE(turbo.state);
  EXPECT_FALSE(light.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
  EXPECT_FALSE(device.get_feature_state(GREE_FEATURE_LIGHT));
  const GreeState transmitted = transmitter.frame(GREE_YAN);
  EXPECT_EQ(transmitted[2] & (GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT), GREE_MODE_BIT_TURBO);
}

TEST(GreeFeatureSwitch, UnsupportedModelFeatureDoesNotTransmitAndRestoresState) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YX1FF, transmitter);
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);

  turbo.turn_on();

  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_FALSE(turbo.state);
  EXPECT_FALSE(device.get_feature_state(GREE_FEATURE_TURBO));
}

TEST(GreeFeatureSwitch, EnablingYB1FATurboSetsEffectiveFanHigh) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);

  turbo.turn_on();

  EXPECT_EQ(transmitter.send_count, 1U);
  EXPECT_TRUE(turbo.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(device.fan_mode.value_or(climate::CLIMATE_FAN_ON), climate::CLIMATE_FAN_HIGH);
  const auto transmitted = decode_transmission(transmitter);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_HIGH);
  EXPECT_TRUE(transmitted.feature_bits & GREE_MODE_BIT_TURBO);
}

TEST(GreeFeatureSwitch, ReceivingYB1FATurboSetsSwitchAndEffectiveFanHigh) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);

  receive_yb1fa_turbo(device);

  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_TRUE(turbo.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(device.fan_mode.value_or(climate::CLIMATE_FAN_ON), climate::CLIMATE_FAN_HIGH);
}

TEST(GreeFeatureSwitch, FanChangeAfterEnablingYB1FATurboClearsTurbo) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);
  turbo.turn_on();

  auto call = device.make_call();
  call.set_fan_mode(climate::CLIMATE_FAN_LOW);
  call.perform();

  EXPECT_EQ(transmitter.send_count, 2U);
  EXPECT_FALSE(turbo.state);
  EXPECT_FALSE(device.get_feature_state(GREE_FEATURE_TURBO));
  const auto transmitted = decode_transmission(transmitter);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_FALSE(transmitted.feature_bits & GREE_MODE_BIT_TURBO);
}

TEST(GreeFeatureSwitch, FanChangeAfterReceivingYB1FATurboClearsTurbo) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  FeatureSwitch<GREE_FEATURE_TURBO> turbo;
  device.register_feature_switch(GREE_FEATURE_TURBO, &turbo);
  receive_yb1fa_turbo(device);

  auto call = device.make_call();
  call.set_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  call.perform();

  EXPECT_EQ(transmitter.send_count, 1U);
  EXPECT_FALSE(turbo.state);
  EXPECT_FALSE(device.get_feature_state(GREE_FEATURE_TURBO));
  const auto transmitted = decode_transmission(transmitter);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_MEDIUM);
  EXPECT_FALSE(transmitted.feature_bits & GREE_MODE_BIT_TURBO);
}

}  // namespace esphome::gree::testing
