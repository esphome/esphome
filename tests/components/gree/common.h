#pragma once

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree::testing {

class MockRemoteTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  MockRemoteTransmitter() : RemoteTransmitterBase(nullptr) {}

  const remote_base::RawTimings &last_data() const { return this->last_data_; }
  uint32_t last_carrier_frequency() const { return this->last_carrier_frequency_; }

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->last_data_ = this->temp_.get_data();
    this->last_carrier_frequency_ = this->temp_.get_carrier_frequency();
  }

 private:
  remote_base::RawTimings last_data_{};
  uint32_t last_carrier_frequency_{0};
};

class TestableGreeClimate : public GreeClimate {
 public:
  using GreeClimate::on_receive;
  using GreeClimate::transmit_state;

  bool receive(const remote_base::RawTimings &timings, uint32_t tolerance = 25,
               remote_base::ToleranceMode tolerance_mode = remote_base::TOLERANCE_MODE_PERCENTAGE) {
    remote_base::RemoteReceiveData data(timings, tolerance, tolerance_mode);
    return this->on_receive(data);
  }

  void set_mode_bits_for_test(uint8_t mode_bits) { this->mode_bits_ = mode_bits; }
  uint8_t mode_bits_for_test() const { return this->mode_bits_; }
  HorizontalDirections horizontal_default_for_test() const { return this->default_horizontal_direction_; }
  VerticalDirections vertical_default_for_test() const { return this->default_vertical_direction_; }
};

struct RoundTripState {
  climate::ClimateMode mode;
  float target_temperature;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
  uint8_t mode_bits;
  optional<climate::ClimatePreset> preset;
};

inline void apply_round_trip_state(TestableGreeClimate &climate, Model model, const RoundTripState &state) {
  climate.set_model(model);
  climate.set_horizontal_default(HORIZONTAL_DIRECTION_AUTO);
  climate.set_vertical_default(VERTICAL_DIRECTION_AUTO);
  climate.mode = state.mode;
  climate.target_temperature = state.target_temperature;
  climate.fan_mode = state.fan_mode;
  climate.swing_mode = state.swing_mode;
  climate.preset = state.preset;
  climate.set_mode_bits_for_test(state.mode_bits);
}

inline void expect_stable_state(const TestableGreeClimate &lhs, const TestableGreeClimate &rhs) {
  EXPECT_EQ(lhs.mode, rhs.mode);
  EXPECT_FLOAT_EQ(lhs.target_temperature, rhs.target_temperature);
  EXPECT_EQ(lhs.fan_mode, rhs.fan_mode);
  EXPECT_EQ(lhs.swing_mode, rhs.swing_mode);
  EXPECT_EQ(lhs.preset, rhs.preset);
  EXPECT_EQ(lhs.mode_bits_for_test(), rhs.mode_bits_for_test());
  EXPECT_EQ(lhs.horizontal_default_for_test(), rhs.horizontal_default_for_test());
  EXPECT_EQ(lhs.vertical_default_for_test(), rhs.vertical_default_for_test());
}

inline void run_round_trip_once(Model model, const RoundTripState &input) {
  MockRemoteTransmitter source_tx;
  TestableGreeClimate source;
  source.set_transmitter(&source_tx);
  apply_round_trip_state(source, model, input);
  source.transmit_state();

  ASSERT_FALSE(source_tx.last_data().empty());
  ASSERT_EQ(source_tx.last_carrier_frequency(), GREE_IR_FREQUENCY);

  TestableGreeClimate normalized;
  normalized.set_model(model);
  ASSERT_TRUE(normalized.receive(source_tx.last_data()));

  MockRemoteTransmitter normalized_tx;
  normalized.set_transmitter(&normalized_tx);
  normalized.transmit_state();

  ASSERT_FALSE(normalized_tx.last_data().empty());
  ASSERT_EQ(normalized_tx.last_carrier_frequency(), GREE_IR_FREQUENCY);

  TestableGreeClimate stable;
  stable.set_model(model);
  ASSERT_TRUE(stable.receive(normalized_tx.last_data()));

  expect_stable_state(normalized, stable);
}

}  // namespace esphome::gree::testing
