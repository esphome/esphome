#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"
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

class CapturingGreeTransmitter final : public remote_base::RemoteTransmitterBase {
 public:
  CapturingGreeTransmitter() : RemoteTransmitterBase(nullptr) {}

  std::array<uint8_t, 8> frame() const {
    const auto &raw = this->captured_;
    EXPECT_EQ(raw.size(), 140U);
    std::array<uint8_t, 8> state{};
    if (raw.size() != 140)
      return state;

    // Two header timings, 64 timings per data half and eight separator/gap timings.
    for (size_t byte = 0; byte < 8; byte++) {
      const size_t start = byte < 4 ? 2 + byte * 16 : 74 + (byte - 4) * 16;
      for (size_t bit = 0; bit < 8; bit++) {
        const int32_t space = raw[start + bit * 2 + 1];
        EXPECT_TRUE(space == -static_cast<int32_t>(GREE_ZERO_SPACE) || space == -static_cast<int32_t>(GREE_ONE_SPACE));
        if (space == -static_cast<int32_t>(GREE_ONE_SPACE))
          state[byte] |= 1U << bit;
      }
    }
    return state;
  }

 protected:
  void send_internal(uint32_t /*send_times*/, uint32_t /*send_wait*/) override {
    this->captured_ = this->temp_.get_data();
  }

 private:
  remote_base::RawTimings captured_;
};

static std::array<uint8_t, 8> transmit_test_frame(Model model, climate::ClimateMode mode, climate::ClimateFanMode fan,
                                                  climate::ClimateSwingMode swing) {
  CapturingGreeTransmitter transmitter;
  GreeClimate device;
  device.set_model(model);
  device.set_transmitter(&transmitter);
  auto call = device.make_call();
  call.set_mode(mode);
  call.set_target_temperature(22);
  call.set_fan_mode(fan);
  call.set_swing_mode(swing);
  call.perform();
  return transmitter.frame();
}

#ifdef USE_GREE_FAHRENHEIT
// The 65 F frame captured off a real YX1FF remote, sent and received through the codec
TEST(GreeTxRegression, YX1FFFahrenheitMatchesTheCapturedRemoteFrame) {
  const std::array<uint8_t, 8> captured{0x09, 0x02, 0x60, 0x5C, 0x00, 0x20, 0x00, 0x30};

  CapturingGreeTransmitter transmitter;
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
  EXPECT_EQ(transmitter.frame(), captured);

  const auto decoded = GreeClimateCodec::decode(GREE_YX1FF, captured);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_TRUE(decoded->fahrenheit);
  EXPECT_TRUE(decoded->half_degree);
  EXPECT_EQ(decoded->target_temperature, GREE_TEMP_MIN + 2);
}
#endif  // USE_GREE_FAHRENHEIT

TEST(GreeTxRegression, YAC1FB9AdvertisesSleepPreset) {
  GreeClimate device;
  device.set_model(GREE_YAC1FB9);
  const auto traits = device.get_traits();
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_NONE));
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_SLEEP));
}

TEST(GreeTxRegression, YX1FFStillAdvertisesSleepPreset) {
  GreeClimate device;
  device.set_model(GREE_YX1FF);
  const auto traits = device.get_traits();
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_NONE));
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_SLEEP));
}

TEST(GreeTxRegression, YX1FFHighUsesFan3AndTurboWithoutSleep) {
  const std::array<uint8_t, 8> expected{0x39, 0x06, 0x70, 0x50, 0x00, 0x20, 0x00, 0xB0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH,
                                climate::CLIMATE_SWING_OFF),
            expected);
}

TEST(GreeTxRegression, YX1FFMediumUsesFan3WithoutTurbo) {
  const std::array<uint8_t, 8> expected{0x39, 0x06, 0x60, 0x50, 0x00, 0x20, 0x00, 0xB0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_MEDIUM,
                                climate::CLIMATE_SWING_OFF),
            expected);
}

TEST(GreeTxRegression, YX1FFVerticalSwingUsesBothVaneNibbles) {
  const std::array<uint8_t, 8> expected{0x79, 0x06, 0x70, 0x50, 0x11, 0x20, 0x00, 0xC0};
  EXPECT_EQ(transmit_test_frame(GREE_YX1FF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH,
                                climate::CLIMATE_SWING_VERTICAL),
            expected);
}

// Power off only clears ModelA; Turbo and the fan speed stay latched, as in IRremoteESP8266's IRGreeAC::setPower()
TEST(GreeTxRegression, YX1FFPowerOffClearsModelABit) {
  const std::array<uint8_t, 8> expected{0x30, 0x06, 0x30, 0x50, 0x00, 0x20, 0x00, 0x20};
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
  const std::array<uint8_t, 8> expected{0x49, 0x06, 0x20, 0x50, 0x01, 0x20, 0x20, 0xD0};
  EXPECT_EQ(transmit_test_frame(GREE_YAC, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO,
                                climate::CLIMATE_SWING_VERTICAL),
            expected);
}

TEST(GreeTxRegression, YACCombinedSwingUsesBothNibbles) {
  const std::array<uint8_t, 8> expected{0x49, 0x06, 0x20, 0x50, 0x11, 0x20, 0x20, 0xE0};
  EXPECT_EQ(
      transmit_test_frame(GREE_YAC, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_BOTH),
      expected);
}

}  // namespace esphome::gree::testing
