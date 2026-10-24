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

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  const remote_base::RemoteTransmitData &data() const { return this->temp_; }
  uint32_t send_count{0};

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override { this->send_count += send_times; }
};

// Expected bytes are fixed vectors, not calculated with the encoder's checksum or model constants.
static void append_frame(remote_base::RawTimings &timings, const std::array<uint8_t, 8> &bytes, int32_t header_space,
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
    // This public feature setter sends the current state without changing any feature bits.
    this->climate_.set_mode_bit(0, false);
  }

  remote_base::RawTimings yaw1f_timings_(const std::array<uint8_t, 8> &command) {
    remote_base::RawTimings timings;
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
  EXPECT_EQ(this->transmitter_.data().get_data(),
            this->yaw1f_timings_({0x00, 0x0D, 0x60, 0x50, 0x00, 0x80, 0x00, 0xF0}));
  EXPECT_EQ(this->transmitter_.data().get_carrier_frequency(), 38000);
  EXPECT_EQ(this->transmitter_.send_count, 1);
}

TEST_F(GreeTransmitTest, Yaw1fUpdatesModeTemperatureFanSwingAndChecksum) {
  this->transmit_(GREE_YAW1F);
  this->climate_.mode = climate::CLIMATE_MODE_COOL;
  this->climate_.target_temperature = 26;
  this->climate_.fan_mode = climate::CLIMATE_FAN_HIGH;
  this->climate_.swing_mode = climate::CLIMATE_SWING_BOTH;
  this->transmit_(GREE_YAW1F);
  EXPECT_EQ(this->transmitter_.data().get_data(),
            this->yaw1f_timings_({0x79, 0x0A, 0x60, 0x50, 0x11, 0x80, 0x00, 0x60}));
  EXPECT_EQ(this->transmitter_.send_count, 2);
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
    remote_base::RawTimings expected;
    const bool yac1fb9 = vector.model == GREE_YAC1FB9;
    append_frame(expected, vector.bytes, yac1fb9 ? 4500 : 4000, yac1fb9 ? 19980 : 19000);
    expected.push_back(0);
    EXPECT_EQ(this->transmitter_.data().get_data(), expected);
    EXPECT_EQ(this->transmitter_.data().get_carrier_frequency(), 38000);
  }
  EXPECT_EQ(this->transmitter_.send_count, 7);
}

}  // namespace esphome::gree::testing
