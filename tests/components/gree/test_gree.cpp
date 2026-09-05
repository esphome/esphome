#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/components/gree/gree.h"

namespace esphome::gree {

using remote_base::RawTimings;
using remote_base::RemoteReceiveData;
using remote_base::RemoteTransmitData;

static optional<GreeState> decode_signal(const RawTimings &raw) {
  return GreeProtocol(GREE_YX1FF).decode(RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
}

static RawTimings encode_signal(const GreeState &state) {
  RemoteTransmitData data;
  GreeProtocol(GREE_YX1FF).encode(&data, state);
  return data.get_data();
}

static GreeClimateData decode_climate_state(Model model, const GreeState &state) {
  auto decoded = GreeClimateCodec::decode(model, state);
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(GreeClimateData{climate::CLIMATE_MODE_OFF, GREE_TEMP_MIN, climate::CLIMATE_FAN_AUTO,
                                          climate::CLIMATE_SWING_OFF, climate::CLIMATE_PRESET_NONE});
}

static GreeClimateData decode_climate_state(const GreeState &state) { return decode_climate_state(GREE_YX1FF, state); }

TEST(GreeYX1FF, DecodeCool22Auto) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  const auto decoded = decode_climate_state(state);

  EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decoded.target_temperature, 22);
  EXPECT_EQ(decoded.fan_mode, climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(decoded.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(GreeYX1FF, DecodeTemperatures) {
  constexpr std::array<uint8_t, 5> temperatures{16, 17, 22, 27, 30};

  for (const uint8_t temperature : temperatures) {
    GreeState state{0x09, static_cast<uint8_t>(temperature - GREE_TEMP_MIN), 0x60, 0x50, 0x00, 0x20, 0x00, 0x00};
    state[7] = GreeProtocol::calculate_checksum(state);
    const auto decoded = decode_climate_state(state);
    EXPECT_EQ(decoded.target_temperature, temperature);
  }
}

TEST(GreeYX1FF, DecodeAllFanSpeeds) {
  struct FanCase {
    uint8_t state0;
    uint8_t state2;
    climate::ClimateFanMode expected;
  };
  constexpr std::array<FanCase, 5> cases{{
      {0x09, 0x60, climate::CLIMATE_FAN_AUTO},
      {0x19, 0x60, climate::CLIMATE_FAN_QUIET},
      {0x29, 0x60, climate::CLIMATE_FAN_LOW},
      {0x39, 0x60, climate::CLIMATE_FAN_MEDIUM},
      {0x39, 0x70, climate::CLIMATE_FAN_HIGH},
  }};

  for (const auto &test : cases) {
    GreeState state{test.state0, 0x06, test.state2, 0x50, 0x00, 0x20, 0x00, 0x00};
    state[7] = GreeProtocol::calculate_checksum(state);
    const auto decoded = decode_climate_state(state);
    EXPECT_EQ(decoded.fan_mode, test.expected);
  }
}

TEST(GreeYX1FF, DecodeSwingOffAndOn) {
  const GreeState swing_off{0x39, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0xB0};
  const GreeState swing_on{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};

  const auto off = decode_climate_state(swing_off);
  const auto on = decode_climate_state(swing_on);

  EXPECT_EQ(off.swing_mode, climate::CLIMATE_SWING_OFF);
  EXPECT_EQ(on.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(GreeYX1FF, DecodeModes) {
  struct ModeCase {
    GreeState state;
    climate::ClimateMode expected;
  };
  const std::array<ModeCase, 5> cases{{
      {{0x38, 0x09, 0x60, 0x50, 0x00, 0x20, 0x00, 0xD0}, climate::CLIMATE_MODE_HEAT_COOL},
      {{0x39, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0xB0}, climate::CLIMATE_MODE_COOL},
      {{0x5A, 0x04, 0x60, 0x50, 0x11, 0x20, 0x00, 0xB0}, climate::CLIMATE_MODE_DRY},
      {{0x5B, 0x08, 0x60, 0x50, 0x11, 0x20, 0x00, 0x00}, climate::CLIMATE_MODE_FAN_ONLY},
      {{0x0C, 0x0C, 0x60, 0x50, 0x00, 0x20, 0x00, 0x40}, climate::CLIMATE_MODE_HEAT},
  }};

  for (const auto &test : cases) {
    const auto decoded = decode_climate_state(test.state);
    EXPECT_EQ(decoded.mode, test.expected);
  }
}

TEST(GreeYX1FF, DecodeOffAndRememberedFields) {
  const GreeState state{0x31, 0x06, 0x30, 0x50, 0x00, 0x20, 0x00, 0x30};
  const auto decoded = decode_climate_state(state);

  EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(decoded.target_temperature, 22);
  EXPECT_EQ(decoded.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(GreeYX1FF, ExactTransmitStatesMatchCaptures) {
  const GreeClimateData cool_auto{climate::CLIMATE_MODE_COOL,   22,
                                  climate::CLIMATE_FAN_AUTO,    climate::CLIMATE_SWING_OFF,
                                  climate::CLIMATE_PRESET_NONE, GREE_LIGHT_BIT};
  const GreeClimateData cool_turbo_swing{climate::CLIMATE_MODE_COOL,   22,
                                         climate::CLIMATE_FAN_HIGH,    climate::CLIMATE_SWING_VERTICAL,
                                         climate::CLIMATE_PRESET_NONE, GREE_LIGHT_BIT};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_YX1FF, cool_auto),
            (GreeState{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YX1FF, cool_turbo_swing),
            (GreeState{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0}));
}

TEST(GreeYB1FA, DecodeCapturedPowerTemperatureAndModes) {
  struct Capture {
    GreeState state;
    climate::ClimateMode mode;
    uint8_t temperature;
    climate::ClimateFanMode fan_mode;
    climate::ClimateSwingMode swing_mode;
  };
  const std::array<Capture, 8> captures{{
      {{0x31, 0x04, 0x20, 0x50, 0x00, 0x21, 0x00, 0x10},
       climate::CLIMATE_MODE_OFF,
       20,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
      {{0x39, 0x04, 0x60, 0x50, 0x00, 0x21, 0x00, 0x90},
       climate::CLIMATE_MODE_COOL,
       20,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
      {{0x39, 0x05, 0x60, 0x50, 0x04, 0x21, 0x00, 0xA0},
       climate::CLIMATE_MODE_COOL,
       21,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
      {{0x1A, 0x08, 0x60, 0x50, 0x00, 0x21, 0x00, 0xE0},
       climate::CLIMATE_MODE_DRY,
       24,
       climate::CLIMATE_FAN_LOW,
       climate::CLIMATE_SWING_OFF},
      {{0x3B, 0x06, 0x60, 0x50, 0x04, 0x21, 0x00, 0xD0},
       climate::CLIMATE_MODE_FAN_ONLY,
       22,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
      {{0x3C, 0x06, 0x60, 0x50, 0x00, 0x21, 0x00, 0xE0},
       climate::CLIMATE_MODE_HEAT,
       22,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
      {{0x48, 0x09, 0x60, 0x50, 0x01, 0x21, 0x00, 0xD0},
       climate::CLIMATE_MODE_HEAT_COOL,
       25,
       climate::CLIMATE_FAN_AUTO,
       climate::CLIMATE_SWING_VERTICAL},
      {{0x39, 0x04, 0x60, 0x50, 0x04, 0x20, 0x00, 0x90},
       climate::CLIMATE_MODE_COOL,
       20,
       climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_SWING_OFF},
  }};

  for (const auto &capture : captures) {
    const auto decoded = decode_climate_state(GREE_YB1FA, capture.state);
    EXPECT_EQ(decoded.mode, capture.mode);
    EXPECT_EQ(decoded.target_temperature, capture.temperature);
    EXPECT_EQ(decoded.fan_mode, capture.fan_mode);
    EXPECT_EQ(decoded.swing_mode, capture.swing_mode);
  }
}

TEST(GreeYB1FA, DecodeCapturedSwingPositions) {
  struct Capture {
    GreeState state;
    climate::ClimateSwingMode expected;
  };
  const std::array<Capture, 10> captures{{
      {{0x39, 0x04, 0x60, 0x50, 0x00, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x79, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_VERTICAL},
      {{0x39, 0x04, 0x60, 0x50, 0x02, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x39, 0x04, 0x60, 0x50, 0x03, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x39, 0x04, 0x60, 0x50, 0x04, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x39, 0x04, 0x60, 0x50, 0x05, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x39, 0x04, 0x60, 0x50, 0x06, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_OFF},
      {{0x79, 0x04, 0x60, 0x50, 0x07, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_VERTICAL},
      {{0x79, 0x04, 0x60, 0x50, 0x09, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_VERTICAL},
      {{0x79, 0x04, 0x60, 0x50, 0x0B, 0x21, 0x00, 0x90}, climate::CLIMATE_SWING_VERTICAL},
  }};

  for (const auto &capture : captures) {
    const auto decoded = decode_climate_state(GREE_YB1FA, capture.state);
    EXPECT_EQ(decoded.swing_mode, capture.expected);
  }
}

TEST(GreeYB1FA, DecodeCapturedFanTurboAndFeatureFields) {
  struct Capture {
    GreeState state;
    climate::ClimateFanMode expected;
    uint8_t feature_bits;
  };
  const std::array<Capture, 7> captures{{
      {{0x59, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_LOW, GREE_LIGHT_BIT},
      {{0x69, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_MEDIUM, GREE_LIGHT_BIT},
      {{0x79, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_HIGH, GREE_LIGHT_BIT},
      {{0x49, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_AUTO, GREE_LIGHT_BIT},
      {{0x49, 0x04, 0x70, 0x50, 0x01, 0x21, 0x00, 0x90},
       climate::CLIMATE_FAN_HIGH,
       GREE_FAN_TURBO_BIT | GREE_LIGHT_BIT},
      {{0x49, 0x04, 0xE0, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_AUTO, GREE_LIGHT_BIT | GREE_XFAN_BIT},
      {{0x79, 0x04, 0x40, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_HIGH, 0},
  }};

  for (const auto &capture : captures) {
    const auto decoded = decode_climate_state(GREE_YB1FA, capture.state);
    EXPECT_EQ(decoded.fan_mode, capture.expected);
    EXPECT_EQ(decoded.feature_bits, capture.feature_bits);
  }
}

TEST(GreeYB1FA, DecodeCapturedTimerFramesAndRejectAuxiliaryFrames) {
  constexpr std::array<GreeState, 2> climate_frames{{
      {0x79, 0xB4, 0x62, 0x50, 0x01, 0x20, 0x00, 0xB0},
      {0x79, 0xD4, 0x61, 0x50, 0x01, 0x20, 0x00, 0xA0},
  }};
  constexpr std::array<GreeState, 3> auxiliary_frames{{
      {0x79, 0xB4, 0x62, 0x60, 0xF6, 0x0A, 0x00, 0x82},
      {0x79, 0xD4, 0x61, 0x60, 0x00, 0x08, 0x51, 0xD1},
      {0x79, 0xD4, 0x61, 0x60, 0xF5, 0x1A, 0x51, 0xD3},
  }};

  for (const auto &state : climate_frames) {
    const auto decoded = decode_climate_state(GREE_YB1FA, state);
    EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_EQ(decoded.target_temperature, 20);
    EXPECT_EQ(decoded.fan_mode, climate::CLIMATE_FAN_HIGH);
    EXPECT_EQ(decoded.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  }
  for (const auto &state : auxiliary_frames) {
    EXPECT_TRUE(GreeProtocol::valid_checksum(state));
    EXPECT_FALSE(GreeClimateCodec::decode(GREE_YB1FA, state).has_value());
  }
}

TEST(GreeYB1FA, ExactTransmitStateUsesYB1FAFanAndSwingEncoding) {
  const GreeClimateData data{climate::CLIMATE_MODE_COOL,   20,
                             climate::CLIMATE_FAN_HIGH,    climate::CLIMATE_SWING_VERTICAL,
                             climate::CLIMATE_PRESET_NONE, GREE_LIGHT_BIT};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x60, 0x50, 0x01, 0x20, 0x00, 0x90}));
}

TEST(GreeYB1FA, FeatureSwitchBitsPreserveModelA) {
  GreeClimateData data{climate::CLIMATE_MODE_COOL, 20, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL,
                       climate::CLIMATE_PRESET_NONE};

  data.feature_bits = GREE_LIGHT_BIT;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x60, 0x50, 0x01, 0x20, 0x00, 0x90}));
  data.feature_bits = GREE_FAN_TURBO_BIT | GREE_LIGHT_BIT | GREE_XFAN_BIT;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0xF0, 0x50, 0x01, 0x20, 0x00, 0x90}));
  data.feature_bits = 0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x40, 0x50, 0x01, 0x20, 0x00, 0x90}));
}

TEST(GreeClimateCodec, FeatureCapabilitiesAreModelSpecific) {
  GreeClimateData data{
      climate::CLIMATE_MODE_COOL,   20,
      climate::CLIMATE_FAN_AUTO,    climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_PRESET_NONE, GREE_FAN_TURBO_BIT | GREE_LIGHT_BIT | GREE_MODEL_A_BIT | GREE_XFAN_BIT};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_YX1FF, data)[2], GREE_MODEL_A_BIT | GREE_LIGHT_BIT);
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data)[2],
            GREE_MODEL_A_BIT | GREE_FAN_TURBO_BIT | GREE_LIGHT_BIT | GREE_XFAN_BIT);
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAN, data)[2],
            GREE_FAN_TURBO_BIT | GREE_LIGHT_BIT | GREE_MODEL_A_BIT | GREE_XFAN_BIT);
}

TEST(GreeClimateCodec, OtherModelsTransmitStateRegression) {
  GreeClimateData source{climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_VERTICAL,
                         climate::CLIMATE_PRESET_NONE};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_GENERIC, source),
            (GreeState{0x29, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0xB0}));
  source.feature_bits = 0xA0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAN, source), (GreeState{0x29, 0x06, 0xA0, 0x50, 0x01, 0x20, 0x00, 0xB0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAA, source), (GreeState{0x69, 0x06, 0xA0, 0x50, 0x00, 0x20, 0x20, 0xD0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAC, source), (GreeState{0x69, 0x06, 0xA0, 0x50, 0x00, 0x20, 0x20, 0xD0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAC1FB9, source),
            (GreeState{0x69, 0x06, 0xA0, 0x50, 0x00, 0x20, 0x20, 0xD0}));
  source.feature_bits = 0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAG, source), (GreeState{0x69, 0x06, 0x60, 0x50, 0x01, 0x40, 0x00, 0xD0}));
}

TEST(GreeYX1FF, ClimateStateRoundTrip) {
  constexpr std::array<climate::ClimateMode, 6> modes{
      climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
  };
  constexpr std::array<uint8_t, 5> temperatures{16, 17, 22, 27, 30};
  constexpr std::array<climate::ClimateFanMode, 5> fans{
      climate::CLIMATE_FAN_AUTO,   climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH,
  };
  constexpr std::array<climate::ClimateSwingMode, 2> swings{
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  };
  constexpr std::array<climate::ClimatePreset, 2> presets{
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_SLEEP,
  };

  for (const auto mode : modes) {
    for (const auto temperature : temperatures) {
      for (const auto fan : fans) {
        for (const auto swing : swings) {
          for (const auto preset : presets) {
            const GreeClimateData source{mode, temperature, fan, swing, preset, GREE_LIGHT_BIT};
            const GreeState state = GreeClimateCodec::encode(GREE_YX1FF, source);
            const auto decoded = decode_climate_state(state);

            EXPECT_EQ(decoded.mode, source.mode);
            EXPECT_EQ(decoded.target_temperature, source.target_temperature);
            EXPECT_EQ(decoded.fan_mode, source.fan_mode);
            EXPECT_EQ(decoded.swing_mode, source.swing_mode);
            EXPECT_EQ(decoded.preset, source.preset);
            EXPECT_EQ(decoded.feature_bits, source.feature_bits);
          }
        }
      }
    }
  }
}

TEST(GreeYB1FA, ClimateStateRoundTrip) {
  constexpr std::array<climate::ClimateMode, 6> modes{
      climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
  };
  constexpr std::array<uint8_t, 5> temperatures{16, 17, 22, 27, 30};
  constexpr std::array<climate::ClimateFanMode, 4> fans{
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  };
  constexpr std::array<climate::ClimateSwingMode, 2> swings{
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  };
  constexpr std::array<climate::ClimatePreset, 1> presets{
      climate::CLIMATE_PRESET_NONE,
  };

  for (const auto mode : modes) {
    for (const auto temperature : temperatures) {
      for (const auto fan : fans) {
        for (const auto swing : swings) {
          for (const auto preset : presets) {
            const GreeClimateData source{mode, temperature, fan, swing, preset, GREE_LIGHT_BIT};
            const GreeState state = GreeClimateCodec::encode(GREE_YB1FA, source);
            const auto decoded = decode_climate_state(GREE_YB1FA, state);

            EXPECT_EQ(decoded.mode, source.mode);
            EXPECT_EQ(decoded.target_temperature, source.target_temperature);
            EXPECT_EQ(decoded.fan_mode, source.fan_mode);
            EXPECT_EQ(decoded.swing_mode, source.swing_mode);
            EXPECT_EQ(decoded.preset, source.preset);
            EXPECT_EQ(decoded.feature_bits, source.feature_bits);
          }
        }
      }
    }
  }
}

TEST(GreeProtocol, SignalRoundTrip) {
  const GreeState expected{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};
  auto decoded = decode_signal(encode_signal(expected));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(GreeState{}), expected);
}

TEST(GreeProtocol, RejectsCorruptChecksum) {
  GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  state[7] ^= 0x10;
  EXPECT_FALSE(decode_signal(encode_signal(state)).has_value());
  EXPECT_FALSE(GreeClimateCodec::decode(GREE_YX1FF, state).has_value());
}

TEST(GreeProtocol, RejectsWrongSeparator) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(state);
  raw[69] = -static_cast<int32_t>(GREE_ZERO_SPACE);  // Change separator 010 to 000.
  EXPECT_FALSE(decode_signal(raw).has_value());
}

TEST(GreeProtocol, RejectsTruncatedFirstBlock) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(state);
  raw.resize(66);  // Header plus the first 32 data bits.
  EXPECT_FALSE(decode_signal(raw).has_value());
}

TEST(GreeProtocol, RejectsBadHeader) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(state);
  raw[0] = 1000;
  EXPECT_FALSE(decode_signal(raw).has_value());
}

TEST(GreeProtocol, RejectsBadInterBlockGap) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(state);
  raw[73] = -5000;
  EXPECT_FALSE(decode_signal(raw).has_value());
}

TEST(GreeYX1FF, RejectsInvalidStateFields) {
  std::array<GreeState, 8> invalid_states{{
      {0x0F, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid operation mode.
      {0x09, 0x0F, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Temperature above 30 degrees.
      {0x09, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0x00},  // Turbo without fan speed 3.
      {0x49, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Swing flag without swing payload.
      {0x09, 0x06, 0x20, 0x50, 0x00, 0x20, 0x00, 0x00},  // ModelA bit does not match power.
      {0x09, 0x06, 0x6A, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid timer units digit.
      {0x09, 0x66, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid timer tens digit.
      {0x09, 0x56, 0x64, 0x50, 0x00, 0x20, 0x00, 0x00},  // Timer above the 24-hour maximum.
  }};

  for (auto &state : invalid_states) {
    state[7] = GreeProtocol::calculate_checksum(state);
    EXPECT_FALSE(GreeClimateCodec::decode(GREE_YX1FF, state).has_value());
  }
}

TEST(GreeYX1FF, DecodeIgnoresLightAndXFan) {
  constexpr std::array<uint8_t, 4> byte2_values{
      GREE_MODEL_A_BIT,
      GREE_MODEL_A_BIT | GREE_LIGHT_BIT,
      GREE_MODEL_A_BIT | GREE_XFAN_BIT,
      GREE_MODEL_A_BIT | GREE_LIGHT_BIT | GREE_XFAN_BIT,
  };

  for (const uint8_t byte2 : byte2_values) {
    GreeState state{0x09, 0x06, byte2, 0x50, 0x00, 0x20, 0x00, 0x00};
    state[7] = GreeProtocol::calculate_checksum(state);
    const auto decoded = decode_climate_state(state);
    EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_EQ(decoded.target_temperature, 22);
  }
}

TEST(GreeProtocol, DecodesRealCapturedPulseTimings) {
  // Captured GREE frame from a physical remote. Marks and spaces vary around their nominal values.
  const RawTimings raw{
      9008, -4496,  644, -1660, 676, -530,  648, -558,  672, -1636, 646, -1660, 644, -556, 650, -584,  626, -560,
      644,  -580,   628, -1680, 624, -560,  648, -1662, 644, -582,  648, -536,  674, -530, 646, -580,  628, -560,
      670,  -532,   646, -562,  644, -556,  672, -536,  648, -1662, 646, -1660, 652, -554, 644, -558,  672, -538,
      644,  -560,   668, -560,  648, -1638, 668, -536,  644, -1660, 668, -532,  648, -560, 648, -1660, 674, -554,
      622,  -19990, 646, -580,  624, -1660, 648, -556,  648, -558,  674, -556,  622, -560, 644, -564,  668, -536,
      646,  -1662,  646, -1658, 672, -534,  648, -558,  644, -562,  648, -1662, 644, -584, 622, -558,  648, -562,
      668,  -534,   670, -536,  670, -532,  672, -536,  646, -560,  646, -558,  648, -558, 670, -534,  650, -558,
      646,  -560,   646, -560,  668, -1638, 646, -1662, 646, -1660, 646, -1660, 648,
  };
  const GreeState expected{0x19, 0x0A, 0x60, 0x50, 0x02, 0x23, 0x00, 0xF0};

  auto decoded = decode_signal(raw);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(GreeState{}), expected);
}

class CountingRemoteTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CountingRemoteTransmitter() : remote_base::RemoteTransmitterBase(nullptr) {}

  uint32_t send_count{0};
  RawTimings last_raw{};

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    (void) send_wait;
    this->send_count += send_times;
    this->last_raw = this->temp_.get_data();
  }
};

class TestFeatureSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

TEST(GreeYX1FF, ReceivePublishesWithoutRetransmitting) {
  const GreeState state{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};
  const RawTimings raw = encode_signal(state);
  CountingRemoteTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);
  device.current_temperature = 21.5f;

  remote_base::RemoteReceiverListener *listener = &device;
  ASSERT_TRUE(listener->on_receive(RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE)));
  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(device.target_temperature, 22.0f);
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(device.fan_mode.value_or(climate::CLIMATE_FAN_ON), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(device.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  EXPECT_FLOAT_EQ(device.current_temperature, 21.5f);
}

TEST(GreeYX1FF, ReceiveSynchronizesLightSwitchWithoutRetransmitting) {
  CountingRemoteTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);
  TestFeatureSwitch light;
  device.register_feature_switch(GreeFeature::GREE_FEATURE_LIGHT, &light);

  GreeState light_on{0x09, 0x02, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00};
  light_on[7] = GreeProtocol::calculate_checksum(light_on);
  remote_base::RemoteReceiverListener *listener = &device;
  ASSERT_TRUE(
      listener->on_receive(RemoteReceiveData(encode_signal(light_on), 25, remote_base::TOLERANCE_MODE_PERCENTAGE)));
  EXPECT_TRUE(light.state);
  EXPECT_TRUE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);

  GreeState light_off = light_on;
  light_off[2] &= ~GREE_LIGHT_BIT;
  light_off[7] = GreeProtocol::calculate_checksum(light_off);
  ASSERT_TRUE(
      listener->on_receive(RemoteReceiveData(encode_signal(light_off), 25, remote_base::TOLERANCE_MODE_PERCENTAGE)));
  EXPECT_FALSE(light.state);
  EXPECT_FALSE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);
}

TEST(GreeYX1FF, ClimateChangePreservesReceivedLightState) {
  CountingRemoteTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);

  GreeState received{0x09, 0x02, 0x40, 0x50, 0x00, 0x20, 0x00, 0x00};
  received[7] = GreeProtocol::calculate_checksum(received);
  remote_base::RemoteReceiverListener *listener = &device;
  ASSERT_TRUE(
      listener->on_receive(RemoteReceiveData(encode_signal(received), 25, remote_base::TOLERANCE_MODE_PERCENTAGE)));
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(device.target_temperature, 18.0f);
  EXPECT_FALSE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);

  auto call = device.make_call();
  call.set_target_temperature(19);
  call.perform();

  EXPECT_EQ(transmitter.send_count, 1U);
  auto transmitted = GreeProtocol(GREE_YX1FF)
                         .decode(RemoteReceiveData(transmitter.last_raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
  ASSERT_TRUE(transmitted.has_value());
  EXPECT_EQ((*transmitted)[1], 3);
  EXPECT_EQ((*transmitted)[2] & GREE_LIGHT_BIT, 0);
}

}  // namespace esphome::gree
