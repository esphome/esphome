#include <gtest/gtest.h>

#include <array>

#include "esphome/components/gree/switch/gree_switch.h"

namespace esphome::gree {
namespace {

using remote_base::RawTimings;
using remote_base::RemoteReceiveData;

class GreeSwitchTestTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  GreeSwitchTestTransmitter() : remote_base::RemoteTransmitterBase(nullptr) {}

  uint32_t send_count{0};
  RawTimings last_raw{};

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    (void) send_wait;
    this->send_count += send_times;
    this->last_raw = this->temp_.get_data();
  }
};

void initialize_climate(GreeClimate &device, Model model, GreeSwitchTestTransmitter &transmitter) {
  device.set_model(model);
  device.set_transmitter(&transmitter);
  device.mode = climate::CLIMATE_MODE_COOL;
  device.target_temperature = 20;
  device.fan_mode = climate::CLIMATE_FAN_LOW;
  device.swing_mode = climate::CLIMATE_SWING_OFF;
  device.preset = climate::CLIMATE_PRESET_NONE;
}

GreeClimateData decode_transmission(const RawTimings &raw) {
  auto state = GreeProtocol(GREE_YB1FA).decode(RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
  EXPECT_TRUE(state.has_value());
  auto decoded = state.has_value() ? GreeClimateCodec::decode(GREE_YB1FA, *state) : optional<GreeClimateData>{};
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(GreeClimateData{climate::CLIMATE_MODE_OFF, GREE_TEMP_MIN, climate::CLIMATE_FAN_AUTO,
                                          climate::CLIMATE_SWING_OFF, climate::CLIMATE_PRESET_NONE});
}

void register_switch(GreeClimate &device, GreeFeatureSwitch &feature_switch, GreeFeature feature) {
  feature_switch.set_parent(&device);
  device.register_feature_switch(feature, &feature_switch);
}

void receive_yb1fa_turbo(GreeClimate &device) {
  const GreeState state{0x49, 0x04, 0x70, 0x50, 0x01, 0x21, 0x00, 0x90};
  remote_base::RemoteTransmitData encoded;
  GreeProtocol(GREE_YB1FA).encode(&encoded, state);
  const RawTimings raw = encoded.get_data();
  remote_base::RemoteReceiverListener *listener = &device;
  ASSERT_TRUE(listener->on_receive(RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE)));
}

}  // namespace

TEST(GreeFeatureSwitch, RxCapableSetupUsesParentStateWithoutTransmit) {
  constexpr std::array<Model, 2> models{GREE_YB1FA, GREE_YX1FF};

  for (const Model model : models) {
    GreeSwitchTestTransmitter transmitter;
    GreeClimate device;
    initialize_climate(device, model, transmitter);
    GreeFeatureSwitch light("Gree Light Switch", GREE_FEATURE_LIGHT);
    register_switch(device, light, GREE_FEATURE_LIGHT);

    light.setup();

    EXPECT_EQ(transmitter.send_count, 0U);
    EXPECT_EQ(light.restore_mode, switch_::SWITCH_RESTORE_DISABLED);
    EXPECT_TRUE(light.state);
  }
}

TEST(GreeFeatureSwitch, LegacySetupRetainsRestoreAndTransmitBehavior) {
  GreeSwitchTestTransmitter transmitter;
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

TEST(GreeFeatureSwitch, EnablingYB1FATurboSetsEffectiveFanHigh) {
  GreeSwitchTestTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);

  turbo.turn_on();

  EXPECT_EQ(transmitter.send_count, 1U);
  EXPECT_TRUE(turbo.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(*device.fan_mode, climate::CLIMATE_FAN_HIGH);
  const auto transmitted = decode_transmission(transmitter.last_raw);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_HIGH);
  EXPECT_TRUE(transmitted.feature_bits & GREE_FAN_TURBO_BIT);
}

TEST(GreeFeatureSwitch, ReceivingYB1FATurboSetsSwitchAndEffectiveFanHigh) {
  GreeSwitchTestTransmitter transmitter;
  GreeClimate device;
  initialize_climate(device, GREE_YB1FA, transmitter);
  GreeFeatureSwitch turbo("Gree Turbo Switch", GREE_FEATURE_TURBO);
  register_switch(device, turbo, GREE_FEATURE_TURBO);

  receive_yb1fa_turbo(device);

  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_TRUE(turbo.state);
  EXPECT_TRUE(device.get_feature_state(GREE_FEATURE_TURBO));
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(*device.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(GreeFeatureSwitch, FanChangeAfterEnablingYB1FATurboClearsTurbo) {
  GreeSwitchTestTransmitter transmitter;
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
  const auto transmitted = decode_transmission(transmitter.last_raw);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_FALSE(transmitted.feature_bits & GREE_FAN_TURBO_BIT);
}

TEST(GreeFeatureSwitch, FanChangeAfterReceivingYB1FATurboClearsTurbo) {
  GreeSwitchTestTransmitter transmitter;
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
  const auto transmitted = decode_transmission(transmitter.last_raw);
  EXPECT_EQ(transmitted.fan_mode, climate::CLIMATE_FAN_MEDIUM);
  EXPECT_FALSE(transmitted.feature_bits & GREE_FAN_TURBO_BIT);
}

}  // namespace esphome::gree
