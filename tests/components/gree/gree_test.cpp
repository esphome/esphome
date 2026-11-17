#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include "common.h"
#include "esphome/core/helpers.h"

namespace esphome::gree::testing {

TEST(GreeClimateTest, HeatCoolHiddenWithoutHeatByDefault) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(GreeClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

#ifdef USE_GREE_FAHRENHEIT
// Expectations from frames captured off a real YX1FF remote and the panel of a Blueridge mini-split.
TEST(GreeClimateTest, FahrenheitHalfStepsMatchCapturedRemoteFrames) {
  struct Expectation {
    float celsius;
    uint8_t temp_field;
    bool extra_degree;
  };
  const Expectation expectations[] = {
      {18.33f, 2, true},    // 65 F -- captured remote frame b1=02 b3=5C
      {18.89f, 3, false},   // 66 F -- captured remote frame b1=03 b3=58
      {21.67f, 6, false},   // 71 F
      {22.22f, 6, true},    // 72 F
      {25.0f, 9, false},    // 77 F
      {25.56f, 10, false},  // 78 F
  };
  for (const auto &expectation : expectations) {
    const uint8_t half_steps = fahrenheit_half_steps(expectation.celsius);
    EXPECT_EQ(expectation.temp_field, half_steps >> 1) << "celsius " << expectation.celsius;
    EXPECT_EQ(expectation.extra_degree, (half_steps & 1) != 0) << "celsius " << expectation.celsius;
  }
}

TEST(GreeClimateTest, FahrenheitHalfStepsCoverEveryWholeDegreeInRange) {
  // 61..86 F is the range a YX1FF remote offers.
  for (int fahrenheit = 61; fahrenheit <= 86; fahrenheit++) {
    const uint8_t half_steps = fahrenheit_half_steps(fahrenheit_to_celsius(static_cast<float>(fahrenheit)));
    const float encoded = static_cast<float>(GREE_TEMP_MIN) + 0.5f * static_cast<float>(half_steps);
    EXPECT_EQ(fahrenheit, static_cast<int>(celsius_to_fahrenheit(encoded))) << "fahrenheit " << fahrenheit;
  }
}

TEST(GreeClimateTest, FahrenheitHalfStepsHandleNonFiniteTarget) {
  // transmit_state() can run from setup() via restore_state_(), when the target may still be NaN.
  EXPECT_EQ(0, fahrenheit_half_steps(NAN));
}
#endif  // USE_GREE_FAHRENHEIT

// Byte 7 is zeroed in these captured YX1FF frames because calculate_checksum() computes it.
TEST(GreeClimateTest, FrameChecksumMatchesCapturedFrames) {
  EXPECT_EQ(0x30, GreeProtocol::calculate_checksum(GreeState{0x09, 0x02, 0x60, 0x5C, 0x00, 0x20, 0x00, 0x00}));
  EXPECT_EQ(0x70, GreeProtocol::calculate_checksum(GreeState{0x09, 0x0A, 0x60, 0x58, 0x00, 0x20, 0x00, 0x00}));
}

// The removed yan/yx1ff checksum `(b0 << 4) + (b1 << 4) + 0xC0` equals calculate_checksum() for every frame they
// emit.
TEST(GreeClimateTest, FrameChecksumMatchesTheRemovedFoldedFormula) {
  int mismatches = 0;
  for (int byte0 = 0; byte0 < 256; byte0++) {
    for (int byte1 = 0; byte1 < 256; byte1++) {
      // byte 2 low nibble is always 0
      static constexpr uint8_t BYTE2_VALUES[] = {0x00, 0x20, 0x60, 0x70};
      for (const uint8_t byte2 : BYTE2_VALUES) {
        for (uint8_t byte4 = 0; byte4 <= GREE_VDIR_DOWN; byte4++) {
          const GreeState frame{
              static_cast<uint8_t>(byte0), static_cast<uint8_t>(byte1), byte2, 0x50, byte4, 0x20, 0x00, 0x00};
          const uint8_t folded = static_cast<uint8_t>((byte0 << 4) + (byte1 << 4) + 0xC0);
          if (GreeProtocol::calculate_checksum(frame) != folded) {
            mismatches++;
          }
        }
      }
    }
  }
  EXPECT_EQ(0, mismatches);
}

static GreeState transmit_test_frame(Model model, climate::ClimateMode mode, climate::ClimateFanMode fan,
                                     climate::ClimateSwingMode swing) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(model);
  device.set_transmitter(&transmitter);
  auto call = device.make_call();
  call.set_mode(mode);
  call.set_target_temperature(22);
  call.set_fan_mode(fan);
  call.set_swing_mode(swing);
  call.perform();
  return transmitter.frame(model);
}

#ifdef USE_GREE_FAHRENHEIT
// The 65 F frame captured off a real YX1FF remote, sent and received through the codec
TEST(GreeTxRegression, YX1FFFahrenheitMatchesTheCapturedRemoteFrame) {
  const GreeState captured{0x09, 0x02, 0x60, 0x5C, 0x00, 0x20, 0x00, 0x30};

  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_use_fahrenheit(true);
  device.set_transmitter(&transmitter);
  auto call = device.make_call();
  call.set_mode(climate::CLIMATE_MODE_COOL);
  call.set_target_temperature(fahrenheit_to_celsius(65.0f));
  call.set_fan_mode(climate::CLIMATE_FAN_AUTO);
  call.set_swing_mode(climate::CLIMATE_SWING_OFF);
  call.perform();
  EXPECT_EQ(transmitter.frame(GREE_YX1FF), captured);

  const auto decoded = GreeClimateCodec::decode(GREE_YX1FF, captured);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_TRUE(decoded->fahrenheit);
  EXPECT_TRUE(decoded->half_degree);
  EXPECT_EQ(decoded->target_temperature, GREE_TEMP_MIN + 2);
}

// A setpoint received from a remote in Fahrenheit mode must go out again as the same degree
TEST(GreeTxRegression, YX1FFReceivedFahrenheitSetpointIsSentBackUnchanged) {
  for (int fahrenheit = 61; fahrenheit <= 86; fahrenheit++) {
    SCOPED_TRACE(fahrenheit);
    const uint8_t half_steps = fahrenheit_half_steps(fahrenheit_to_celsius(static_cast<float>(fahrenheit)));
    const uint8_t byte3 = 0x50 | GREE_FAHRENHEIT_BIT | ((half_steps & 1) ? GREE_TEMP_HALF_DEGREE_BIT : 0);
    const GreeState received =
        with_checksum({0x09, static_cast<uint8_t>(half_steps >> 1), 0x60, byte3, 0x00, 0x20, 0x00, 0x00});

    CapturingTransmitter transmitter;
    GreeClimate device;
    device.set_model(GREE_YX1FF);
    device.set_use_fahrenheit(true);
    device.set_transmitter(&transmitter);
    ASSERT_TRUE(receive(device, GREE_YX1FF, received));

    auto call = device.make_call();
    call.set_fan_mode(climate::CLIMATE_FAN_LOW);
    call.perform();
    const GreeState sent = transmitter.frame(GREE_YX1FF);
    EXPECT_EQ(sent[1], received[1]);
    EXPECT_EQ(sent[3], received[3]);
  }
}
#endif  // USE_GREE_FAHRENHEIT

TEST(GreeTxRegression, SleepPresetAdvertisedOnYAC1FB9AndYX1FF) {
  for (const Model model : {GREE_YAC1FB9, GREE_YX1FF}) {
    GreeClimate device;
    device.set_model(model);
    const auto traits = device.get_traits();
    EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_NONE)) << "model " << model;
    EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_SLEEP)) << "model " << model;
  }
}

TEST(GreeTxRegression, YX1FFHighUsesFan3AndTurboWithoutSleep) {
  const GreeState expected{0x39, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0xB0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH,
                                climate::CLIMATE_SWING_OFF),
            expected);
}

TEST(GreeTxRegression, YX1FFMediumUsesFan3WithoutTurbo) {
  const GreeState expected{0x39, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_MEDIUM,
                                climate::CLIMATE_SWING_OFF),
            expected);
}

TEST(GreeTxRegression, YX1FFVerticalSwingUsesBothVaneNibbles) {
  const GreeState expected{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH,
                                climate::CLIMATE_SWING_VERTICAL),
            expected);
}

// Power off only clears ModelA; Turbo and the fan speed stay latched, as in IRremoteESP8266's IRGreeAC::setPower()
TEST(GreeTxRegression, YX1FFPowerOffClearsModelABit) {
  const GreeState expected{0x30, 0x06, 0x30, 0x50, 0x00, 0x20, 0x00, 0x20};
  EXPECT_EQ(
      transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_OFF, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF),
      expected);
}

TEST(GreeTxRegression, YX1FFDoesNotAdvertiseHorizontalSwing) {
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  const auto traits = device.get_traits();
  EXPECT_TRUE(traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(GreeTxRegression, YACVerticalVaneIsInLowNibble) {
  const GreeState expected{0x49, 0x06, 0x20, 0x50, 0x01, 0x20, 0x20, 0xD0};
  EXPECT_EQ(transmit_test_frame(GREE_YAC, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO,
                                climate::CLIMATE_SWING_VERTICAL),
            expected);
}

TEST(GreeTxRegression, YACCombinedSwingUsesBothNibbles) {
  const GreeState expected{0x49, 0x06, 0x20, 0x50, 0x11, 0x20, 0x20, 0xE0};
  EXPECT_EQ(
      transmit_test_frame(GREE_YAC, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_BOTH),
      expected);
}

static GreeClimateData decode_climate_state(Model model, const GreeState &state) {
  auto decoded = GreeClimateCodec::decode(model, state);
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(GreeClimateData{climate::CLIMATE_MODE_OFF, GREE_TEMP_MIN, climate::CLIMATE_FAN_AUTO,
                                          climate::CLIMATE_SWING_OFF, climate::CLIMATE_PRESET_NONE});
}

TEST(GreeYX1FF, DecodeCool22Auto) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  const auto decoded = decode_climate_state(GREE_YX1FF, state);

  EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(decoded.target_temperature, 22);
  EXPECT_EQ(decoded.fan_mode, climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(decoded.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(GreeYX1FF, DecodeTemperatures) {
  constexpr std::array<uint8_t, 5> temperatures{16, 17, 22, 27, 30};

  for (const uint8_t temperature : temperatures) {
    const GreeState state =
        with_checksum({0x09, static_cast<uint8_t>(temperature - GREE_TEMP_MIN), 0x60, 0x50, 0x00, 0x20, 0x00, 0x00});
    const auto decoded = decode_climate_state(GREE_YX1FF, state);
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
    const GreeState state = with_checksum({test.state0, 0x06, test.state2, 0x50, 0x00, 0x20, 0x00, 0x00});
    const auto decoded = decode_climate_state(GREE_YX1FF, state);
    EXPECT_EQ(decoded.fan_mode, test.expected);
  }
}

TEST(GreeYX1FF, DecodeSwingOffAndOn) {
  const GreeState swing_off{0x39, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0xB0};
  const GreeState swing_on{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};

  const auto off = decode_climate_state(GREE_YX1FF, swing_off);
  const auto on = decode_climate_state(GREE_YX1FF, swing_on);

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
    const auto decoded = decode_climate_state(GREE_YX1FF, test.state);
    EXPECT_EQ(decoded.mode, test.expected);
  }
}

TEST(GreeYX1FF, DecodeOffAndRememberedFields) {
  const GreeState state{0x31, 0x06, 0x30, 0x50, 0x00, 0x20, 0x00, 0x30};
  const auto decoded = decode_climate_state(GREE_YX1FF, state);

  EXPECT_EQ(decoded.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(decoded.target_temperature, 22);
  EXPECT_EQ(decoded.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(GreeYX1FF, ExactTransmitStatesMatchCaptures) {
  const GreeClimateData cool_auto{climate::CLIMATE_MODE_COOL,   22,
                                  climate::CLIMATE_FAN_AUTO,    climate::CLIMATE_SWING_OFF,
                                  climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};
  const GreeClimateData cool_turbo_swing{climate::CLIMATE_MODE_COOL,   22,
                                         climate::CLIMATE_FAN_HIGH,    climate::CLIMATE_SWING_VERTICAL,
                                         climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};

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
      {{0x59, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_LOW, GREE_MODE_BIT_LIGHT},
      {{0x69, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_MEDIUM, GREE_MODE_BIT_LIGHT},
      {{0x79, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_HIGH, GREE_MODE_BIT_LIGHT},
      {{0x49, 0x04, 0x60, 0x50, 0x01, 0x21, 0x00, 0x90}, climate::CLIMATE_FAN_AUTO, GREE_MODE_BIT_LIGHT},
      {{0x49, 0x04, 0x70, 0x50, 0x01, 0x21, 0x00, 0x90},
       climate::CLIMATE_FAN_HIGH,
       GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT},
      {{0x49, 0x04, 0xE0, 0x50, 0x01, 0x21, 0x00, 0x90},
       climate::CLIMATE_FAN_AUTO,
       GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN},
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
                             climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x60, 0x50, 0x01, 0x20, 0x00, 0x90}));
}

TEST(GreeYB1FA, FeatureSwitchBitsPreserveModelA) {
  GreeClimateData data{climate::CLIMATE_MODE_COOL, 20, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL,
                       climate::CLIMATE_PRESET_NONE};

  data.feature_bits = GREE_MODE_BIT_LIGHT;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x60, 0x50, 0x01, 0x20, 0x00, 0x90}));
  data.feature_bits = GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0xF0, 0x50, 0x01, 0x20, 0x00, 0x90}));
  data.feature_bits = 0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data), (GreeState{0x79, 0x04, 0x40, 0x50, 0x01, 0x20, 0x00, 0x90}));
}

TEST(GreeClimateCodec, FeatureCapabilitiesAreModelSpecific) {
  GreeClimateData data{climate::CLIMATE_MODE_COOL,
                       20,
                       climate::CLIMATE_FAN_AUTO,
                       climate::CLIMATE_SWING_OFF,
                       climate::CLIMATE_PRESET_NONE,
                       GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH | GREE_MODE_BIT_XFAN};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_YX1FF, data)[2], GREE_MODEL_A_BIT | GREE_MODE_BIT_LIGHT);
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YB1FA, data)[2],
            GREE_MODEL_A_BIT | GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN);
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAN, data)[2],
            GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_HEALTH | GREE_MODE_BIT_XFAN);
}

TEST(GreeClimateCodec, OtherModelsTransmitStateRegression) {
  GreeClimateData source{climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_VERTICAL,
                         climate::CLIMATE_PRESET_NONE};

  EXPECT_EQ(GreeClimateCodec::encode(GREE_GENERIC, source),
            (GreeState{0x29, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0xB0}));
  source.feature_bits = 0xA0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAN, source), (GreeState{0x29, 0x06, 0xA0, 0x50, 0x01, 0x20, 0x00, 0xB0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAA, source), (GreeState{0x69, 0x06, 0xA0, 0x50, 0x00, 0x20, 0x20, 0xD0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAC, source), (GreeState{0x69, 0x06, 0xA0, 0x50, 0x01, 0x20, 0x20, 0xD0}));
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAC1FB9, source),
            (GreeState{0x69, 0x06, 0xA0, 0x50, 0x00, 0x20, 0x20, 0xD0}));
  source.feature_bits = 0;
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAG, source), (GreeState{0x69, 0x06, 0x60, 0x50, 0x01, 0x40, 0x00, 0xD0}));
}

TEST(GreeClimateCodec, LegacyModelsClimateStateRoundTrip) {
  struct LegacyCase {
    Model model;
    GreeClimateData source;
  };

  const std::array<LegacyCase, 8> cases{{
      {GREE_GENERIC,
       {climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_PRESET_NONE}},
      {GREE_YAN,
       {climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN}},
      {GREE_YAA,
       {climate::CLIMATE_MODE_COOL, 21, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN}},
      {GREE_YAC,
       {climate::CLIMATE_MODE_COOL, 23, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_HORIZONTAL,
        climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT}},
      {GREE_YAC,
       {climate::CLIMATE_MODE_HEAT, 25, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_BOTH,
        climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_TURBO | GREE_MODE_BIT_LIGHT}},
      {GREE_YAC1FB9,
       {climate::CLIMATE_MODE_DRY, 20, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT}},
      {GREE_YAG,
       {climate::CLIMATE_MODE_FAN_ONLY, 22, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_HORIZONTAL,
        climate::CLIMATE_PRESET_NONE}},
      {GREE_YAG,
       {climate::CLIMATE_MODE_COOL, 19, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_BOTH,
        climate::CLIMATE_PRESET_NONE}},
  }};

  for (const auto &test : cases) {
    const GreeState state = GreeClimateCodec::encode(test.model, test.source);
    const auto decoded = decode_climate_state(test.model, state);

    EXPECT_EQ(decoded.mode, test.source.mode);
    EXPECT_EQ(decoded.target_temperature, test.source.target_temperature);
    EXPECT_EQ(decoded.fan_mode, test.source.fan_mode);
    EXPECT_EQ(decoded.swing_mode, test.source.swing_mode);
    EXPECT_EQ(decoded.feature_bits, test.source.feature_bits);
  }
}

TEST(GreeYAA, DecodeAcceptsOemTimerAndAuxiliaryFields) {
  GreeClimateData source{climate::CLIMATE_MODE_COOL,   22,
                         climate::CLIMATE_FAN_HIGH,    climate::CLIMATE_SWING_VERTICAL,
                         climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN};
  GreeState state = GreeClimateCodec::encode(GREE_YAA, source);

  // Keep the standard mode/fan/temp/feature fields but populate fields that vary between OEM remotes.
  state[1] |= 0xB0;  // Timer enabled, half-hour flag, and timer tens.
  state[2] = (state[2] & 0xF0) | 0x08;
  state[3] = 0x5C;  // Fahrenheit/extra-degree flags plus the usual upper nibble.
  state[4] = GREE_VDIR_SWING_UP;
  state[5] = 0x66;  // Display temperature, I-Feel, fixed bits, and WiFi.
  state[6] = 0xA5;

  const auto decoded = decode_climate_state(GREE_YAA, with_checksum(state));
  EXPECT_EQ(decoded.mode, source.mode);
  EXPECT_EQ(decoded.target_temperature, source.target_temperature);
  EXPECT_EQ(decoded.fan_mode, source.fan_mode);
  EXPECT_EQ(decoded.swing_mode, source.swing_mode);
  EXPECT_EQ(decoded.feature_bits, source.feature_bits);
}

TEST(GreeProtocol, YAC1FB9SignalRoundTrip) {
  const GreeClimateData source{climate::CLIMATE_MODE_COOL,   22,
                               climate::CLIMATE_FAN_MEDIUM,  climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};
  const GreeState expected = GreeClimateCodec::encode(GREE_YAC1FB9, source);
  auto decoded = decode_signal(GREE_YAC1FB9, encode_signal(GREE_YAC1FB9, expected));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(GreeState{}), expected);
}

TEST(GreeYAC1FB9, SleepPresetCodecAndIRRoundTrip) {
  GreeClimateData data{climate::CLIMATE_MODE_COOL,   22,
                       climate::CLIMATE_FAN_MEDIUM,  climate::CLIMATE_SWING_VERTICAL,
                       climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};
  const GreeState awake = GreeClimateCodec::encode(GREE_YAC1FB9, data);
  EXPECT_EQ(awake[0] & GREE_PRESET_SLEEP_BIT, 0U);

  data.preset = climate::CLIMATE_PRESET_SLEEP;
  const GreeState asleep = GreeClimateCodec::encode(GREE_YAC1FB9, data);
  EXPECT_EQ(asleep[0], static_cast<uint8_t>(awake[0] | GREE_PRESET_SLEEP_BIT));
  EXPECT_EQ(asleep[7], awake[7]);  // Sleep does not affect the checksum's low-nibble sum.
  EXPECT_TRUE(GreeProtocol::valid_checksum(asleep));

  const auto wire_frame = decode_signal(GREE_YAC1FB9, encode_signal(GREE_YAC1FB9, asleep));
  ASSERT_TRUE(wire_frame.has_value());
  EXPECT_EQ(*wire_frame, asleep);

  const auto decoded_sleep = GreeClimateCodec::decode(GREE_YAC1FB9, *wire_frame);
  ASSERT_TRUE(decoded_sleep.has_value());
  EXPECT_EQ(decoded_sleep->preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_EQ(decoded_sleep->mode, data.mode);
  EXPECT_EQ(decoded_sleep->target_temperature, data.target_temperature);

  const auto decoded_awake = GreeClimateCodec::decode(GREE_YAC1FB9, awake);
  ASSERT_TRUE(decoded_awake.has_value());
  EXPECT_EQ(decoded_awake->preset, climate::CLIMATE_PRESET_NONE);
}

TEST(GreeYAC1FB9, ReceivedSleepPresetUpdatesClimate) {
  GreeClimateData data{climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF,
                       climate::CLIMATE_PRESET_SLEEP};
  GreeClimate device;
  device.set_model(GREE_YAC1FB9);

  EXPECT_TRUE(receive(device, GREE_YAC1FB9, GreeClimateCodec::encode(GREE_YAC1FB9, data)));
  EXPECT_EQ(device.preset, climate::CLIMATE_PRESET_SLEEP);

  data.preset = climate::CLIMATE_PRESET_NONE;
  EXPECT_TRUE(receive(device, GREE_YAC1FB9, GreeClimateCodec::encode(GREE_YAC1FB9, data)));
  EXPECT_EQ(device.preset, climate::CLIMATE_PRESET_NONE);
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
            const GreeClimateData source{mode, temperature, fan, swing, preset, GREE_MODE_BIT_LIGHT};
            const GreeState state = GreeClimateCodec::encode(GREE_YX1FF, source);
            const auto decoded = decode_climate_state(GREE_YX1FF, state);

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
            const GreeClimateData source{mode, temperature, fan, swing, preset, GREE_MODE_BIT_LIGHT};
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
  auto decoded = decode_signal(GREE_YX1FF, encode_signal(GREE_YX1FF, expected));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(GreeState{}), expected);
}

TEST(GreeProtocol, RejectsCorruptChecksum) {
  GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  state[7] ^= 0x10;
  EXPECT_FALSE(decode_signal(GREE_YX1FF, encode_signal(GREE_YX1FF, state)).has_value());
}

TEST(GreeProtocol, RejectsWrongSeparator) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(GREE_YX1FF, state);
  raw[69] = -static_cast<int32_t>(GREE_ZERO_SPACE);  // Change separator 010 to 000.
  EXPECT_FALSE(decode_signal(GREE_YX1FF, raw).has_value());
}

TEST(GreeProtocol, RejectsTruncatedFirstBlock) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(GREE_YX1FF, state);
  raw.resize(66);  // Header plus the first 32 data bits.
  EXPECT_FALSE(decode_signal(GREE_YX1FF, raw).has_value());
}

TEST(GreeProtocol, RejectsBadHeader) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(GREE_YX1FF, state);
  raw[0] = 1000;
  EXPECT_FALSE(decode_signal(GREE_YX1FF, raw).has_value());
}

TEST(GreeProtocol, RejectsBadInterBlockGap) {
  const GreeState state{0x09, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  auto raw = encode_signal(GREE_YX1FF, state);
  raw[73] = -5000;
  EXPECT_FALSE(decode_signal(GREE_YX1FF, raw).has_value());
}

TEST(GreeYX1FF, RejectsInvalidStateFields) {
  constexpr std::array<GreeState, 8> invalid_states{{
      {0x0F, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid operation mode.
      {0x09, 0x0F, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Temperature above 30 degrees.
      {0x09, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0x00},  // Turbo without fan speed 3.
      {0x49, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Swing flag without swing payload.
      {0x09, 0x06, 0x20, 0x50, 0x00, 0x20, 0x00, 0x00},  // ModelA bit does not match power.
      {0x09, 0x06, 0x6A, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid timer units digit.
      {0x09, 0x66, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00},  // Invalid timer tens digit.
      {0x09, 0x56, 0x64, 0x50, 0x00, 0x20, 0x00, 0x00},  // Timer above the 24-hour maximum.
  }};

  for (const auto &state : invalid_states)
    EXPECT_FALSE(GreeClimateCodec::decode(GREE_YX1FF, with_checksum(state)).has_value());
}

TEST(GreeYX1FF, DecodeIgnoresLightAndXFan) {
  constexpr std::array<uint8_t, 4> byte2_values{
      GREE_MODEL_A_BIT,
      GREE_MODEL_A_BIT | GREE_MODE_BIT_LIGHT,
      GREE_MODEL_A_BIT | GREE_MODE_BIT_XFAN,
      GREE_MODEL_A_BIT | GREE_MODE_BIT_LIGHT | GREE_MODE_BIT_XFAN,
  };

  for (const uint8_t byte2 : byte2_values) {
    const auto decoded =
        decode_climate_state(GREE_YX1FF, with_checksum({0x09, 0x06, byte2, 0x50, 0x00, 0x20, 0x00, 0x00}));
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

  auto decoded = decode_signal(GREE_YX1FF, raw);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(GreeState{}), expected);
}

class TestFeatureSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

TEST(GreeYAA, ReceivePublishesStateAndFeatureBitsWithoutRetransmitting) {
  const GreeClimateData source{climate::CLIMATE_MODE_COOL,   22,
                               climate::CLIMATE_FAN_HIGH,    climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_PRESET_NONE, GREE_MODE_BIT_LIGHT};
  const GreeState state = GreeClimateCodec::encode(GREE_YAA, source);
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YAA);
  device.set_transmitter(&transmitter);
  TestFeatureSwitch light;
  device.register_feature_switch(GreeFeature::GREE_FEATURE_LIGHT, &light);

  ASSERT_TRUE(receive(device, GREE_YAA, state));
  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_EQ(device.mode, source.mode);
  EXPECT_FLOAT_EQ(device.target_temperature, source.target_temperature);
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(device.fan_mode.value_or(climate::CLIMATE_FAN_ON), source.fan_mode);
  EXPECT_EQ(device.swing_mode, source.swing_mode);
  EXPECT_TRUE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_TRUE(light.state);
}

TEST(GreeYX1FF, ReceivePublishesWithoutRetransmitting) {
  const GreeState state{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);
  device.current_temperature = 21.5f;

  ASSERT_TRUE(receive(device, GREE_YX1FF, state));
  EXPECT_EQ(transmitter.send_count, 0U);
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(device.target_temperature, 22.0f);
  ASSERT_TRUE(device.fan_mode.has_value());
  EXPECT_EQ(device.fan_mode.value_or(climate::CLIMATE_FAN_ON), climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(device.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  EXPECT_FLOAT_EQ(device.current_temperature, 21.5f);
}

TEST(GreeYX1FF, ReceiveSynchronizesLightSwitchWithoutRetransmitting) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);
  TestFeatureSwitch light;
  device.register_feature_switch(GreeFeature::GREE_FEATURE_LIGHT, &light);

  const GreeState light_on = with_checksum({0x09, 0x02, 0x60, 0x50, 0x00, 0x20, 0x00, 0x00});
  ASSERT_TRUE(receive(device, GREE_YX1FF, light_on));
  EXPECT_TRUE(light.state);
  EXPECT_TRUE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);

  GreeState light_off = light_on;
  light_off[2] &= ~GREE_MODE_BIT_LIGHT;
  ASSERT_TRUE(receive(device, GREE_YX1FF, with_checksum(light_off)));
  EXPECT_FALSE(light.state);
  EXPECT_FALSE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);
}

TEST(GreeYX1FF, ClimateChangePreservesReceivedLightState) {
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  device.set_transmitter(&transmitter);

  ASSERT_TRUE(receive(device, GREE_YX1FF, with_checksum({0x09, 0x02, 0x40, 0x50, 0x00, 0x20, 0x00, 0x00})));
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(device.target_temperature, 18.0f);
  EXPECT_FALSE(device.get_feature_state(GreeFeature::GREE_FEATURE_LIGHT));
  EXPECT_EQ(transmitter.send_count, 0U);

  auto call = device.make_call();
  call.set_target_temperature(19);
  call.perform();

  EXPECT_EQ(transmitter.send_count, 1U);
  const GreeState transmitted_state = transmitter.frame(GREE_YX1FF);
  EXPECT_EQ(transmitted_state[1], 3);
  EXPECT_EQ(transmitted_state[2] & GREE_MODE_BIT_LIGHT, 0);
}

TEST(GreeYAC, EncodeSwingAxesInVaneFields) {
  struct SwingCase {
    climate::ClimateSwingMode mode;
    uint8_t expected_vanes;
  };
  const std::array<SwingCase, 4> cases{{
      {climate::CLIMATE_SWING_OFF, 0x00},
      {climate::CLIMATE_SWING_VERTICAL, 0x01},
      {climate::CLIMATE_SWING_HORIZONTAL, 0x10},
      {climate::CLIMATE_SWING_BOTH, 0x11},
  }};
  GreeClimateData data{climate::CLIMATE_MODE_COOL, 23, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
                       climate::CLIMATE_PRESET_NONE};
  for (const auto &test : cases) {
    data.swing_mode = test.mode;
    const GreeState encoded = GreeClimateCodec::encode(GREE_YAC, data);
    EXPECT_EQ(encoded[4], test.expected_vanes);
    EXPECT_EQ(decode_climate_state(GREE_YAC, encoded).swing_mode, test.mode);
  }
}

TEST(GreeYAC, CapturedYAC1FBSwingAndAuxiliaryFrames) {
  struct SwingCase {
    GreeState state;
    climate::ClimateSwingMode expected;
  };
  const std::array<SwingCase, 4> cases{{
      {{0x5C, 0x03, 0x60, 0x50, 0x11, 0x40, 0x00, 0xE0}, climate::CLIMATE_SWING_BOTH},
      {{0x1C, 0x03, 0x60, 0x50, 0x01, 0x40, 0x00, 0xD0}, climate::CLIMATE_SWING_VERTICAL},
      {{0x1C, 0x03, 0x60, 0x50, 0x10, 0x40, 0x00, 0xE0}, climate::CLIMATE_SWING_HORIZONTAL},
      {{0x79, 0x03, 0x60, 0x50, 0x16, 0x40, 0x00, 0x00}, climate::CLIMATE_SWING_HORIZONTAL},
  }};
  for (const auto &test : cases)
    EXPECT_EQ(decode_climate_state(GREE_YAC, with_checksum(test.state)).swing_mode, test.expected);
  const std::array<GreeState, 2> auxiliary{{
      {0x5C, 0x03, 0x60, 0x70, 0x00, 0x00, 0x10, 0xA0},
      {0x1C, 0x03, 0x60, 0x70, 0x00, 0x00, 0x10, 0xA0},
  }};
  for (const auto &state : auxiliary) {
    ASSERT_TRUE(GreeProtocol::valid_checksum(state));
    EXPECT_FALSE(GreeClimateCodec::decode(GREE_YAC, state).has_value());
  }
}

TEST(GreeYAC, AuxiliaryFrameDoesNotOverwriteSwing) {
  const GreeState first{0x5C, 0x03, 0x60, 0x50, 0x11, 0x40, 0x00, 0xE0};
  const GreeState second{0x5C, 0x03, 0x60, 0x70, 0x00, 0x00, 0x10, 0xA0};
  CapturingTransmitter transmitter;
  GreeClimate device;
  device.set_model(GREE_YAC);
  device.set_transmitter(&transmitter);
  ASSERT_TRUE(receive(device, GREE_YAC, first));
  EXPECT_EQ(device.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_FALSE(receive(device, GREE_YAC, second));
  EXPECT_EQ(device.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_EQ(transmitter.send_count, 0U);
}

static void append_yaw1f_wire_frame(RawTimings &raw, const GreeState &bytes) {
  // Built by hand from the captured wire timings, independent of GreeProtocol::encode().
  raw.push_back(9000);
  raw.push_back(-4500);
  for (size_t byte = 0; byte < bytes.size(); byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      raw.push_back(620);
      raw.push_back(bytes[byte] & (1U << bit) ? -1600 : -540);
    }
    if (byte == 3)
      raw.insert(raw.end(), {620, -540, 620, -1600, 620, -540, 620, -19980});
  }
  raw.push_back(620);
}

TEST(GreeYAW1F, CapturedOffCommandAndFixedFollowupMatchWireTimings) {
  const GreeClimateData data{climate::CLIMATE_MODE_OFF, 29, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
                             climate::CLIMATE_PRESET_NONE};
  const GreeState command{0x00, 0x0D, 0x60, 0x50, 0x00, 0x80, 0x00, 0xF0};
  const GreeState auxiliary{0, 0, 0, 0xA0, 0, 0, 0, 0xA0};
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAW1F, data), command);
  RawTimings expected;
  append_yaw1f_wire_frame(expected, command);
  expected.push_back(-40000);
  append_yaw1f_wire_frame(expected, auxiliary);
  expected.push_back(0);
  const RawTimings encoded = encode_signal(GREE_YAW1F, command);
  EXPECT_EQ(encoded.size(), 280U);
  EXPECT_EQ(encoded, expected);
  const auto decoded = decode_signal(GREE_YAW1F, encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), command);
}

TEST(GreeYAW1F, SwingFanAndChecksumMatchCapturedFormat) {
  const GreeClimateData data{climate::CLIMATE_MODE_COOL, 26, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_BOTH,
                             climate::CLIMATE_PRESET_NONE};
  const GreeState command{0x79, 0x0A, 0x60, 0x50, 0x11, 0x80, 0x00, 0x60};
  EXPECT_EQ(GreeClimateCodec::encode(GREE_YAW1F, data), command);
  EXPECT_TRUE(GreeProtocol::valid_checksum(command));
  const auto decoded = decode_signal(GREE_YAW1F, encode_signal(GREE_YAW1F, command));
  ASSERT_TRUE(decoded.has_value());
  const auto climate_state = decode_climate_state(GREE_YAW1F, *decoded);
  EXPECT_EQ(climate_state.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(climate_state.target_temperature, 26);
  EXPECT_EQ(climate_state.fan_mode, climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(climate_state.swing_mode, climate::CLIMATE_SWING_BOTH);
}

TEST(GreeYAW1F, SeparateAuxiliaryDoesNotChangeReceivedClimate) {
  const GreeState command{0x79, 0x0A, 0x60, 0x50, 0x11, 0x80, 0x00, 0x60};
  const GreeState auxiliary{0, 0, 0, 0xA0, 0, 0, 0, 0xA0};
  // Receivers with idle < 40 ms supply two separate captures.
  RawTimings first;
  append_yaw1f_wire_frame(first, command);
  first.push_back(0);
  RawTimings second;
  append_yaw1f_wire_frame(second, auxiliary);
  second.push_back(0);
  GreeClimate device;
  device.set_model(GREE_YAW1F);
  EXPECT_TRUE(receive_raw(device, first));
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(device.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_FALSE(receive_raw(device, second));
  EXPECT_EQ(device.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FALSE(GreeClimateCodec::decode(GREE_YAW1F, auxiliary).has_value());
}

TEST(GreeYAW1F, WrongCombinedAuxiliaryIsRejected) {
  const GreeState command{0x79, 0x0A, 0x60, 0x50, 0x11, 0x80, 0x00, 0x60};
  RawTimings raw = encode_signal(GREE_YAW1F, command);
  ASSERT_EQ(raw.size(), 280U);
  // Second frame byte 3 bit 0: change the expected A0 into A1.
  raw[142 + 3 * 16 + 1] = -1600;
  EXPECT_FALSE(decode_signal(GREE_YAW1F, raw).has_value());
}

// Expected bytes are fixed vectors, not calculated with the encoder's checksum or model constants.
static void append_frame(RawTimings &timings, const std::array<uint8_t, 8> &bytes, int32_t header_space,
                         int32_t message_space) {
  timings.push_back(9000);
  timings.push_back(-header_space);
  for (size_t i = 0; i < bytes.size(); i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      timings.push_back(620);
      timings.push_back(bytes[i] & (1 << bit) ? -1600 : -540);
    }
    if (i == 3) {
      timings.insert(timings.end(), {620, -540, 620, -1600, 620, -540, 620, -message_space});
    }
  }
  timings.push_back(620);
}

// Sends through GreeClimate::transmit_state(), the path a real state change takes.
class GreeTransmitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->climate_.set_transmitter(&this->transmitter_);
    this->climate_.mode = climate::CLIMATE_MODE_OFF;
    this->climate_.target_temperature = 29;
    this->climate_.fan_mode = climate::CLIMATE_FAN_AUTO;
    this->climate_.swing_mode = climate::CLIMATE_SWING_OFF;
  }

  void transmit_(Model model) {
    this->climate_.set_model(model);
    // An empty call changes nothing and sends the current state.
    this->climate_.make_call().perform();
  }

  RawTimings yaw1f_timings_(const std::array<uint8_t, 8> &command) {
    RawTimings timings;
    append_frame(timings, command, 4500, 19980);
    timings.push_back(-40000);
    append_frame(timings, {0x00, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0xA0}, 4500, 19980);
    timings.push_back(0);
    return timings;
  }

  GreeClimate climate_;
  CapturingTransmitter transmitter_;
};

TEST_F(GreeTransmitTest, Yaw1fMatchesCapturedOffCommandAndFollowup) {
  this->transmit_(GREE_YAW1F);
  // Captured from the flashed device on 26 September: off, 29 degrees, fan auto, swing off.
  EXPECT_EQ(this->transmitter_.last_raw, this->yaw1f_timings_({0x00, 0x0D, 0x60, 0x50, 0x00, 0x80, 0x00, 0xF0}));
  EXPECT_EQ(this->transmitter_.last_carrier_frequency, 38000U);
  EXPECT_EQ(this->transmitter_.send_count, 1U);
}

TEST_F(GreeTransmitTest, Yaw1fUpdatesModeTemperatureFanSwingAndChecksum) {
  this->transmit_(GREE_YAW1F);
  this->climate_.mode = climate::CLIMATE_MODE_COOL;
  this->climate_.target_temperature = 26;
  this->climate_.fan_mode = climate::CLIMATE_FAN_HIGH;
  this->climate_.swing_mode = climate::CLIMATE_SWING_BOTH;
  this->transmit_(GREE_YAW1F);
  EXPECT_EQ(this->transmitter_.last_raw, this->yaw1f_timings_({0x79, 0x0A, 0x60, 0x50, 0x11, 0x80, 0x00, 0x60}));
  EXPECT_EQ(this->transmitter_.send_count, 2U);
}

TEST_F(GreeTransmitTest, ExistingModelsKeepTheirBytesAndSingleMessageFraming) {
  struct ModelVector {
    Model model;
    std::array<uint8_t, 8> bytes;
  };
  const ModelVector vectors[] = {
      {GREE_GENERIC, {0x00, 0x0D, 0x00, 0x00, 0x00, 0x20, 0x00, 0x90}},
      // Byte 2 carries the display light bit, which is on until a switch turns it off.
      {GREE_YAN, {0x00, 0x0D, 0x20, 0x50, 0x00, 0x20, 0x00, 0x90}},
      {GREE_YAA, {0x00, 0x0D, 0x20, 0x50, 0x00, 0x20, 0x20, 0xB0}},
      {GREE_YAC, {0x00, 0x0D, 0x20, 0x50, 0x00, 0x20, 0x20, 0xB0}},
      {GREE_YAC1FB9, {0x00, 0x0D, 0x20, 0x50, 0x00, 0x20, 0x20, 0xB0}},
      {GREE_YX1FF, {0x00, 0x0D, 0x20, 0x50, 0x00, 0x20, 0x00, 0x90}},
      {GREE_YAG, {0x00, 0x0D, 0x60, 0x50, 0x00, 0x40, 0x00, 0xB0}},
  };
  for (const auto &vector : vectors) {
    SCOPED_TRACE(static_cast<int>(vector.model));
    this->transmit_(vector.model);
    RawTimings expected;
    const bool yac1fb9 = vector.model == GREE_YAC1FB9;
    append_frame(expected, vector.bytes, yac1fb9 ? 4500 : 4000, yac1fb9 ? 19980 : 19000);
    expected.push_back(0);
    EXPECT_EQ(this->transmitter_.last_raw, expected);
    EXPECT_EQ(this->transmitter_.last_carrier_frequency, 38000U);
  }
  EXPECT_EQ(this->transmitter_.send_count, 7U);
}

}  // namespace esphome::gree::testing
