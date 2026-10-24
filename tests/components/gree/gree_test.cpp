#include <gtest/gtest.h>

#include <array>
#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"

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
