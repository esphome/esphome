#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/gree/switch/gree_switch.h"

namespace esphome::gree::testing {
namespace {

void initialize_climate(GreeClimate &device, Model model, CapturingTransmitter &transmitter) {
  device.set_model(model);
  device.set_transmitter(&transmitter);
  device.mode = climate::CLIMATE_MODE_COOL;
  device.target_temperature = 20;
  device.fan_mode = climate::CLIMATE_FAN_LOW;
  device.swing_mode = climate::CLIMATE_SWING_OFF;
  device.preset = climate::CLIMATE_PRESET_NONE;
}

GreeClimateData decode_transmission(const CapturingTransmitter &transmitter) {
  auto decoded = GreeClimateCodec::decode(GREE_YB1FA, transmitter.frame(GREE_YB1FA));
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(GreeClimateData{climate::CLIMATE_MODE_OFF, GREE_TEMP_MIN, climate::CLIMATE_FAN_AUTO,
                                          climate::CLIMATE_SWING_OFF, climate::CLIMATE_PRESET_NONE});
}

void register_switch(GreeClimate &device, GreeFeatureSwitch &feature_switch, GreeFeature feature) {
  feature_switch.set_parent(&device);
  device.register_feature_switch(feature, &feature_switch);
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
    initialize_climate(device, model, transmitter);
    GreeFeatureSwitch light("Gree Light Switch", GREE_FEATURE_LIGHT);
    register_switch(device, light, GREE_FEATURE_LIGHT);
    light.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);

    light.setup();

    EXPECT_EQ(transmitter.send_count, 0U);
    EXPECT_TRUE(light.state);
  }
}

TEST(GreeFeatureSwitch, LegacySetupRetainsRestoreAndTransmitBehavior) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YAN, transmitter);
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);
  turbo.set_restore_mode(switch_::SWITCH_ALWAYS_ON);

  turbo.setup();

  EXPECT_EQ(transmitter.send_count, 1U);
  EXPECT_TRUE(turbo.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
}

TEST(GreeFeatureSwitch, UnsupportedModelFeatureDoesNotTransmitAndRestoresState) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YX1FF, transmitter);
  GreeFeatureSwitch turbo("Unsupported Gree Turbo", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);

  turbo.turn_on();

  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_FALSE(turbo.state);
  EXPECT_FALSE(device.get_feature_state(GREE_FEATURE_TURBO));
}

TEST(GreeFeatureSwitch, EnablingYB1FATurboSetsEffectiveFanHigh) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);

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
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);

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
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);
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
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);
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
