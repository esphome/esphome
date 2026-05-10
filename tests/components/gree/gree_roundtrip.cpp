#include "common.h"

#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace esphome::gree::testing {

namespace {

std::string model_to_string(Model model) {
  switch (model) {
    case GREE_GENERIC:
      return "generic";
    case GREE_YAN:
      return "yan";
    case GREE_YAA:
      return "yaa";
    case GREE_YAC:
      return "yac";
    case GREE_YAC1FB9:
      return "yac1fb9";
    case GREE_YX1FF:
      return "yx1ff";
    case GREE_YAG:
      return "yag";
    case GREE_YAP1F:
      return "yap1f";
    default:
      return "unknown";
  }
}

std::vector<climate::ClimateFanMode> fan_modes_for_model(Model model) {
  std::vector<climate::ClimateFanMode> fan_modes{
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  };
  if (model == GREE_YX1FF) {
    fan_modes.push_back(climate::CLIMATE_FAN_QUIET);
  }
  return fan_modes;
}

std::vector<RoundTripState> build_state_matrix(Model model) {
  constexpr std::array<climate::ClimateMode, 6> modes{
      climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
  };
  constexpr std::array<float, 3> temperatures{16.0f, 25.0f, 30.0f};
  constexpr std::array<climate::ClimateSwingMode, 4> swing_modes{
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_HORIZONTAL,
      climate::CLIMATE_SWING_BOTH,
  };
  constexpr std::array<uint8_t, 3> mode_bits{
      0x00,
      GREE_MODE_BIT_LIGHT,
      static_cast<uint8_t>(GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH | GREE_MODE_BIT_XFAN),
  };

  const auto fan_modes = fan_modes_for_model(model);
  std::vector<RoundTripState> states{};
  states.reserve(modes.size() * temperatures.size() * fan_modes.size() * swing_modes.size() * mode_bits.size());

  for (const auto mode : modes) {
    for (const float target_temperature : temperatures) {
      for (const auto fan_mode : fan_modes) {
        for (const auto swing_mode : swing_modes) {
          for (const auto bits : mode_bits) {
            RoundTripState state{};
            state.mode = mode;
            state.target_temperature = target_temperature;
            state.fan_mode = fan_mode;
            state.swing_mode = swing_mode;
            state.mode_bits = bits;
            state.preset = {};

            states.push_back(state);
          }
        }
      }
    }
  }

  return states;
}

std::string state_to_string(const RoundTripState &state) {
  std::ostringstream out;
  out << "mode=" << static_cast<int>(state.mode);
  out << " temp=" << state.target_temperature;
  out << " fan=" << static_cast<int>(state.fan_mode);
  out << " swing=" << static_cast<int>(state.swing_mode);
  out << " bits=0x" << std::hex << static_cast<int>(state.mode_bits);
  if (state.preset.has_value()) {
    out << " preset=" << static_cast<int>(state.preset.value());
  }
  return out.str();
}

std::array<uint8_t, GREE_STATE_FRAME_SIZE> decode_standard_frame(const remote_base::RawTimings &timings) {
  std::array<uint8_t, GREE_STATE_FRAME_SIZE> buffer{};
  size_t pos = 2;  // Header mark/space.

  const auto read_byte = [&timings, &pos]() {
    uint8_t value = 0;
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      EXPECT_LT(pos + 1, timings.size());
      EXPECT_NEAR(timings[pos], GREE_BIT_MARK, 1);
      const int32_t space = -timings[pos + 1];
      if (space > static_cast<int32_t>((GREE_ONE_SPACE + GREE_ZERO_SPACE) / 2)) {
        value |= mask;
      }
      pos += 2;
    }
    return value;
  };

  for (size_t i = 0; i < 4; i++) {
    buffer[i] = read_byte();
  }

  pos += 8;  // Fixed 010 bits and inter-message gap.

  for (size_t i = 4; i < GREE_STATE_FRAME_SIZE; i++) {
    buffer[i] = read_byte();
  }

  return buffer;
}

}  // namespace

TEST(GreeRoundTripTest, GreeToGreeToGreeRoundTripAcrossAllBuiltinModels) {
  constexpr std::array<Model, 8> models{
      GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAC1FB9, GREE_YX1FF, GREE_YAG, GREE_YAP1F,
  };

  for (const auto model : models) {
    for (const auto &state : build_state_matrix(model)) {
      SCOPED_TRACE("model=" + model_to_string(model) + " " + state_to_string(state));
      run_round_trip_once(model, state);
    }
  }
}

TEST(GreeRoundTripTest, HeatpumpIRCompatibleGreeProtocolsRoundTripWithGreeReceiver) {
  constexpr std::array<Model, 5> heatpumpir_gree_models{
      GREE_GENERIC, GREE_YAN, GREE_YAA, GREE_YAC, GREE_YAP1F,
  };

  for (const auto model : heatpumpir_gree_models) {
    for (const auto &state : build_state_matrix(model)) {
      SCOPED_TRACE("heatpumpir_protocol_model=" + model_to_string(model) + " " + state_to_string(state));
      run_round_trip_once(model, state);
    }
  }
}

TEST(GreeEncodingTest, YacFixedHorizontalDirectionContributesToStandardChecksum) {
  MockRemoteTransmitter source_tx;
  TestableGreeClimate source;
  source.set_transmitter(&source_tx);
  source.set_model(GREE_YAC);
  source.set_horizontal_default(HORIZONTAL_DIRECTION_RIGHT);
  source.mode = climate::CLIMATE_MODE_COOL;
  source.target_temperature = 25.0f;
  source.fan_mode = climate::CLIMATE_FAN_AUTO;
  source.swing_mode = climate::CLIMATE_SWING_OFF;
  source.set_mode_bits_for_test(GREE_MODE_BIT_LIGHT);

  source.transmit_state();

  ASSERT_FALSE(source_tx.last_data().empty());
  const auto frame = decode_standard_frame(source_tx.last_data());

  EXPECT_EQ(frame[4], 0x60);
  EXPECT_EQ(frame[7], 0x60);
}

}  // namespace esphome::gree::testing
