#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GreeHeatpumpIR.h"
#include "esphome/components/remote_base/remote_base.h"

#if __has_include("../esphome/components/gree/gree.h")

namespace esphome::heatpumpir::testing {

namespace {

using ::heatpumpir_test_stub::MockRemoteTransmitter;

enum ProtocolVariant {
  PROTOCOL_GREE,
  PROTOCOL_GREEYAA,
  PROTOCOL_GREEYAN,
  PROTOCOL_GREEYAC,
  PROTOCOL_GREEYAP,
};

class IRSenderESPHomeBridge : public IRSender {
 public:
  explicit IRSenderESPHomeBridge(remote_base::RemoteTransmitterBase *transmitter)
      : IRSender(0), transmit_(transmitter->transmit()) {}

  void setFrequency(int frequency) override { this->transmit_.get_data()->set_carrier_frequency(1000 * frequency); }
  void space(int space_length) override { this->transmit_.get_data()->space(space_length > 0 ? space_length : 0); }
  void mark(int mark_length) override { this->transmit_.get_data()->mark(mark_length > 0 ? mark_length : 0); }
  void perform() { this->transmit_.perform(); }

 private:
  remote_base::RemoteTransmitterBase::TransmitCall transmit_;
};

class TestableGreeClimate : public gree::GreeClimate {
 public:
  using GreeClimate::on_receive;
  using GreeClimate::transmit_state;

  bool receive(const remote_base::RawTimings &timings, uint32_t tolerance = 25,
               remote_base::ToleranceMode tolerance_mode = remote_base::TOLERANCE_MODE_PERCENTAGE) {
    remote_base::RemoteReceiveData data(timings, tolerance, tolerance_mode);
    return this->on_receive(data);
  }

  uint8_t mode_bits_for_test() const { return this->mode_bits_; }
  gree::HorizontalDirections horizontal_default_for_test() const { return this->default_horizontal_direction_; }
  gree::VerticalDirections vertical_default_for_test() const { return this->default_vertical_direction_; }
};

struct ProtocolModelPair {
  ProtocolVariant protocol;
  gree::Model model;
  const char *name;
  bool supports_light;
};

struct SourceState {
  climate::ClimateMode mode;
  float target_temperature;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
  bool light;
};

std::string state_to_string(const SourceState &state) {
  std::ostringstream out;
  out << "mode=" << static_cast<int>(state.mode);
  out << " temp=" << state.target_temperature;
  out << " fan=" << static_cast<int>(state.fan_mode);
  out << " swing=" << static_cast<int>(state.swing_mode);
  out << " light=" << state.light;
  return out.str();
}

std::vector<SourceState> build_state_matrix(bool include_light) {
  constexpr std::array<climate::ClimateMode, 6> modes{
      climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
  };
  constexpr std::array<float, 3> temperatures{16.0f, 25.0f, 30.0f};
  constexpr std::array<climate::ClimateFanMode, 4> fan_modes{
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  };
  constexpr std::array<climate::ClimateSwingMode, 4> swing_modes{
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
      climate::CLIMATE_SWING_BOTH,
  };

  std::vector<SourceState> states{};
  const size_t light_states = include_light ? 2 : 1;
  states.reserve(modes.size() * temperatures.size() * fan_modes.size() * swing_modes.size() * light_states);

  for (const auto mode : modes) {
    for (const auto temperature : temperatures) {
      for (const auto fan_mode : fan_modes) {
        for (const auto swing_mode : swing_modes) {
          if (include_light) {
            states.push_back(SourceState{mode, temperature, fan_mode, swing_mode, false});
            states.push_back(SourceState{mode, temperature, fan_mode, swing_mode, true});
          } else {
            states.push_back(SourceState{mode, temperature, fan_mode, swing_mode, true});
          }
        }
      }
    }
  }

  return states;
}

std::unique_ptr<HeatpumpIR> make_backend(ProtocolVariant protocol) {
  switch (protocol) {
    case PROTOCOL_GREE:
      return std::make_unique<GreeGenericHeatpumpIR>();
    case PROTOCOL_GREEYAA:
      return std::make_unique<GreeYAAHeatpumpIR>();
    case PROTOCOL_GREEYAN:
      return std::make_unique<GreeYANHeatpumpIR>();
    case PROTOCOL_GREEYAC:
      return std::make_unique<GreeYACHeatpumpIR>();
    case PROTOCOL_GREEYAP:
      return std::make_unique<GreeYAPHeatpumpIR>();
    default:
      return nullptr;
  }
}

void emit_heatpumpir_state(remote_base::RemoteTransmitterBase *transmitter, ProtocolVariant protocol,
                           const SourceState &state) {
  auto backend = make_backend(protocol);
  ASSERT_NE(backend, nullptr);

  uint8_t power_mode_cmd = POWER_OFF;
  uint8_t operating_mode_cmd = MODE_AUTO;
  uint8_t temperature_cmd =
      static_cast<uint8_t>(std::clamp<float>(state.target_temperature, gree::GREE_TEMP_MIN, gree::GREE_TEMP_MAX));
  uint8_t fan_speed_cmd = FAN_AUTO;

  uint8_t swing_v_cmd = VDIR_UP;
  if (state.swing_mode == climate::CLIMATE_SWING_VERTICAL || state.swing_mode == climate::CLIMATE_SWING_BOTH) {
    swing_v_cmd = VDIR_SWING;
  }

  uint8_t swing_h_cmd = HDIR_MIDDLE;
  if (state.swing_mode == climate::CLIMATE_SWING_HORIZONTAL || state.swing_mode == climate::CLIMATE_SWING_BOTH) {
    swing_h_cmd = HDIR_SWING;
  }

  switch (state.fan_mode) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed_cmd = FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_cmd = FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_cmd = FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed_cmd = FAN_AUTO;
      break;
  }

  switch (state.mode) {
    case climate::CLIMATE_MODE_COOL:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power_mode_cmd = POWER_OFF;
      operating_mode_cmd = MODE_AUTO;
      break;
  }

  IRSenderESPHomeBridge sender(transmitter);
  if (protocol == PROTOCOL_GREEYAP) {
    auto *gree_yap = static_cast<GreeYAPHeatpumpIR *>(backend.get());
    gree_yap->send(sender, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd,
                   false, false, state.light);
  } else {
    backend->send(sender, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd);
  }
  sender.perform();
}

void expect_stable_state(const TestableGreeClimate &lhs, const TestableGreeClimate &rhs) {
  EXPECT_EQ(lhs.mode, rhs.mode);
  EXPECT_FLOAT_EQ(lhs.target_temperature, rhs.target_temperature);
  EXPECT_EQ(lhs.fan_mode, rhs.fan_mode);
  EXPECT_EQ(lhs.swing_mode, rhs.swing_mode);
  EXPECT_EQ(lhs.preset, rhs.preset);
  EXPECT_EQ(lhs.mode_bits_for_test(), rhs.mode_bits_for_test());
  EXPECT_EQ(lhs.horizontal_default_for_test(), rhs.horizontal_default_for_test());
  EXPECT_EQ(lhs.vertical_default_for_test(), rhs.vertical_default_for_test());
}

void run_round_trip_once(const ProtocolModelPair &pair, const SourceState &input) {
  MockRemoteTransmitter source_tx;
  emit_heatpumpir_state(&source_tx, pair.protocol, input);

  ASSERT_FALSE(source_tx.last_data().empty());
  ASSERT_EQ(source_tx.last_carrier_frequency(), gree::GREE_IR_FREQUENCY);

  TestableGreeClimate normalized;
  normalized.set_model(pair.model);
  ASSERT_TRUE(normalized.receive(source_tx.last_data()));

  MockRemoteTransmitter normalized_tx;
  normalized.set_transmitter(&normalized_tx);
  normalized.transmit_state();

  ASSERT_FALSE(normalized_tx.last_data().empty());
  ASSERT_EQ(normalized_tx.last_carrier_frequency(), gree::GREE_IR_FREQUENCY);

  TestableGreeClimate stable;
  stable.set_model(pair.model);
  ASSERT_TRUE(stable.receive(normalized_tx.last_data()));

  expect_stable_state(normalized, stable);
}

}  // namespace

TEST(HeatpumpIRRoundTripTest, HeatpumpIRToGreeToGreeAcrossGreeProtocolVariants) {
  constexpr std::array<ProtocolModelPair, 5> pairs{
      ProtocolModelPair{PROTOCOL_GREE, gree::GREE_GENERIC, "gree", false},
      ProtocolModelPair{PROTOCOL_GREEYAN, gree::GREE_YAN, "greeyan", false},
      ProtocolModelPair{PROTOCOL_GREEYAA, gree::GREE_YAA, "greeya", false},
      ProtocolModelPair{PROTOCOL_GREEYAC, gree::GREE_YAC, "greeyac", false},
      ProtocolModelPair{PROTOCOL_GREEYAP, gree::GREE_YAP1F, "greeyap", true},
  };

  for (const auto &pair : pairs) {
    for (const auto &state : build_state_matrix(pair.supports_light)) {
      SCOPED_TRACE(std::string("protocol=") + pair.name + " " + state_to_string(state));
      run_round_trip_once(pair, state);
    }
  }
}

}  // namespace esphome::heatpumpir::testing
#else
TEST(HeatpumpIRRoundTripTest, HeatpumpIRToGreeToGreeAcrossGreeProtocolVariants) {
  GTEST_SKIP() << "Gree component is not available in this C++ unit-test run";
}
#endif
