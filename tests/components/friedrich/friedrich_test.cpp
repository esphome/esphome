#include <gtest/gtest.h>
#include <array>
#include <vector>

#include "esphome/components/friedrich/friedrich.h"
#include "esphome/components/remote_base/aeha_protocol.h"

namespace esphome::friedrich::testing {

using remote_base::AEHAData;
using remote_base::AEHAProtocol;

// Records the last transmission instead of sending it
class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  int sends{0};
  remote_base::RawTimings last;

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->sends++;
    this->last = this->temp_.get_data();
  }
};

class TestFriedrich : public FriedrichClimate {
 public:
  using FriedrichClimate::on_receive;
  using FriedrichClimate::setup;
  using FriedrichClimate::traits;
  using FriedrichClimate::transmit_state;
};

constexpr uint16_t ADDRESS = 0x28C6;
// Byte 6 holds the temperature code (bit 7 is the power flag), byte 7 the mode, byte 8 the fan
constexpr uint8_t TEMP_72_CODE = 0x06;

struct Fixture {
  TestFriedrich sut;
  CapturingTransmitter transmitter;

  Fixture() { this->sut.set_transmitter(&this->transmitter); }

  // The last transmission decoded, or an empty frame if it could not be decoded
  AEHAData sent() {
    remote_base::RemoteReceiveData rx(this->transmitter.last, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    return AEHAProtocol().decode(rx).value_or(AEHAData{});
  }

  bool receive(const AEHAData &frame) {
    remote_base::RemoteTransmitData tx;
    AEHAProtocol().encode(&tx, frame);
    remote_base::RemoteReceiveData rx(tx.get_data(), 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    return this->sut.on_receive(rx);
  }
};

static uint8_t reverse(uint8_t v) {
  uint8_t r = 0;
  for (int i = 0; i < 8; i++)
    r |= ((v >> i) & 1) << (7 - i);
  return r;
}

// Independent implementation of the 14 byte frame checksum
static AEHAData state_frame(uint8_t temp_code, uint8_t mode, uint8_t fan = 0x00) {
  AEHAData f{ADDRESS, {0x00, 0x08, 0x08, 0x7F, 0x90, 0x0C, (uint8_t) (temp_code | 0x80), mode, fan, 0, 0, 0, 0x04, 0}};
  uint8_t sum = 0;
  for (int i = 6; i < 13; i++)
    sum += reverse(f.data[i]);
  f.data[13] = reverse((uint8_t) (208 - sum));
  return f;
}

static AEHAData off_frame() { return AEHAData{ADDRESS, {0x00, 0x08, 0x08, 0x40, 0xBF}}; }

TEST(FriedrichTests, TraitsReportFahrenheit) {
  Fixture f;
  auto traits = f.sut.traits();
  EXPECT_EQ(traits.get_temperature_unit(), TemperatureUnit::FAHRENHEIT);
  EXPECT_FLOAT_EQ(traits.get_visual_min_temperature(), 60.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_max_temperature(), 88.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_target_temperature_step(), 2.0f);
}

TEST(FriedrichTests, OffIsAlwaysSentAsFiveByteFrame) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_OFF;
  f.sut.transmit_state();
  ASSERT_EQ(f.transmitter.sends, 1);
  EXPECT_EQ(f.sent(), off_frame());
}

TEST(FriedrichTests, StateFrameEncodesFahrenheitTarget) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  f.sut.target_temperature = 72;
  f.sut.fan_mode = climate::CLIMATE_FAN_AUTO;
  f.sut.swing_mode = climate::CLIMATE_SWING_OFF;
  f.sut.transmit_state();
  EXPECT_EQ(f.sent(), state_frame(TEMP_72_CODE, 0x80));
}

TEST(FriedrichTests, OddTargetStepsUpToEven) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  f.sut.target_temperature = 71;
  f.sut.transmit_state();
  auto data = f.sent();
  ASSERT_EQ(data.data.size(), STATE_MESSAGE_LENGTH);
  EXPECT_EQ(data.data[6], TEMP_72_CODE | 0x80);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);  // stored value matches what was sent
}

TEST(FriedrichTests, NonHeatModesAreRaisedToSixtyFour) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  f.sut.target_temperature = 60;
  f.sut.transmit_state();
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 64.0f);
  auto data = f.sent();
  ASSERT_EQ(data.data.size(), STATE_MESSAGE_LENGTH);
  EXPECT_EQ(data.data[6], 0x04 | 0x80);  // 64 F

  f.sut.mode = climate::CLIMATE_MODE_HEAT;
  f.sut.target_temperature = 60;
  f.sut.transmit_state();
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 60.0f);
}

TEST(FriedrichTests, ReceivesStateFrame) {
  Fixture f;
  EXPECT_TRUE(f.receive(state_frame(TEMP_72_CODE, 0x20, 0x80)));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);
  EXPECT_EQ(f.sut.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(FriedrichTests, ReceivesOffFrame) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  EXPECT_TRUE(f.receive(off_frame()));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_OFF);
}

TEST(FriedrichTests, IgnoresOtherAddress) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  auto frame = off_frame();
  frame.address = 0x1234;
  EXPECT_FALSE(f.receive(frame));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
}

TEST(FriedrichTests, IgnoresBadHeaderBytes) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  auto frame = off_frame();
  frame.data[1] = 0x09;
  EXPECT_FALSE(f.receive(frame));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
}

TEST(FriedrichTests, IgnoresUnknownModeWithoutChangingState) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  f.sut.target_temperature = 80;
  EXPECT_FALSE(f.receive(state_frame(TEMP_72_CODE, 0xD0)));  // MIN_HEAT is not exposed
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 80.0f);
}

TEST(FriedrichTests, IgnoresBadChecksum) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  auto frame = state_frame(TEMP_72_CODE, 0x20);
  frame.data[13] ^= 0xFF;
  EXPECT_FALSE(f.receive(frame));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
}

TEST(FriedrichTests, OwnTransmissionIsAccepted) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_FAN_ONLY;
  f.sut.target_temperature = 76;
  f.sut.fan_mode = climate::CLIMATE_FAN_LOW;
  f.sut.swing_mode = climate::CLIMATE_SWING_VERTICAL;
  f.sut.transmit_state();

  Fixture g;
  remote_base::RemoteReceiveData rx(f.transmitter.last, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  EXPECT_TRUE(g.sut.on_receive(rx));
  EXPECT_EQ(g.sut.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(g.sut.target_temperature, 76.0f);
  EXPECT_EQ(g.sut.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(g.sut.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(FriedrichTests, SetupDefaultsTargetToSeventyTwoFahrenheit) {
  Fixture f;
  f.sut.setup();
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_OFF);
}

}  // namespace esphome::friedrich::testing
