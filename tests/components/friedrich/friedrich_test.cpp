#include <gtest/gtest.h>
#include <functional>

#include "esphome/components/friedrich/friedrich.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

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
constexpr uint8_t STATE_MESSAGE_LENGTH = FriedrichClimate::STATE_MESSAGE_LENGTH;
// Byte 6 holds the temperature code (bit 7 is the power flag), byte 7 the mode, byte 8 the fan
constexpr uint8_t TEMP_72_CODE = 0x06;

struct Fixture {
  TestFriedrich sut;
  CapturingTransmitter transmitter;

  Fixture() {
    // Climate tests share one preference key, so clear what an earlier test saved or setup restores it
    if (global_preferences != nullptr)
      global_preferences->reset();
    this->sut.set_transmitter(&this->transmitter);
  }

  // The last transmission decoded, or an empty frame if it could not be decoded
  AEHAData sent() { return AEHAProtocol().decode(rx_of(this->transmitter.last)).value_or(AEHAData{}); }

  bool receive(const AEHAData &frame) {
    remote_base::RemoteTransmitData tx;
    AEHAProtocol().encode(&tx, frame);
    return this->sut.on_receive(rx_of(tx.get_data()));
  }

  static remote_base::RemoteReceiveData rx_of(const remote_base::RawTimings &timings) {
    return remote_base::RemoteReceiveData(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  }
};

// Independent implementation of the 14 byte frame checksum
static AEHAData state_frame(uint8_t temp_code, uint8_t mode, uint8_t fan = 0x00) {
  AEHAData f{ADDRESS, {0x00, 0x08, 0x08, 0x7F, 0x90, 0x0C, (uint8_t) (temp_code | 0x80), mode, fan, 0, 0, 0, 0x04, 0}};
  uint8_t sum = 0;
  for (int i = 6; i < 13; i++)
    sum += reverse_bits(f.data[i]);
  f.data[13] = reverse_bits(static_cast<uint8_t>(208 - sum));
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

// Codes captured from the remote
TEST(FriedrichTests, EveryTemperatureEncodesToRemoteCode) {
  static constexpr uint8_t CODES[] = {0x00, 0x08, 0x04, 0x0C, 0x02, 0x0A, 0x06, 0x0E,
                                      0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07};
  for (size_t i = 0; i < sizeof(CODES); i++) {
    const auto fahrenheit = static_cast<uint8_t>(60 + i * 2);
    SCOPED_TRACE(fahrenheit);
    Fixture f;
    f.sut.mode = climate::CLIMATE_MODE_HEAT;
    f.sut.target_temperature = fahrenheit;
    f.sut.transmit_state();
    EXPECT_EQ(f.sent(), state_frame(CODES[i], 0x20));

    Fixture g;
    EXPECT_TRUE(g.receive(state_frame(CODES[i], 0x20)));
    EXPECT_FLOAT_EQ(g.sut.target_temperature, fahrenheit);
  }
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

TEST(FriedrichTests, IgnoresUnknownModeWithoutChangingState) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_COOL;
  f.sut.target_temperature = 80;
  EXPECT_FALSE(f.receive(state_frame(TEMP_72_CODE, 0xD0)));  // MIN_HEAT is not exposed
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 80.0f);
}

TEST(FriedrichTests, IgnoresCorruptedFrames) {
  struct Case {
    const char *name;
    AEHAData frame;
    std::function<void(AEHAData &)> corrupt;
  };
  const Case cases[] = {
      {"other address", off_frame(), [](AEHAData &f) { f.address = 0x1234; }},
      {"bad header byte", off_frame(), [](AEHAData &f) { f.data[1] = 0x09; }},
      {"bad off checksum", off_frame(), [](AEHAData &f) { f.data[4] ^= 0xFF; }},
      {"bad state checksum", state_frame(TEMP_72_CODE, 0x20), [](AEHAData &f) { f.data[13] ^= 0xFF; }},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(c.name);
    Fixture f;
    f.sut.mode = climate::CLIMATE_MODE_COOL;
    AEHAData frame = c.frame;
    c.corrupt(frame);
    EXPECT_FALSE(f.receive(frame));
    EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
  }
}

TEST(FriedrichTests, OwnTransmissionIsAccepted) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_FAN_ONLY;
  f.sut.target_temperature = 76;
  f.sut.fan_mode = climate::CLIMATE_FAN_LOW;
  f.sut.swing_mode = climate::CLIMATE_SWING_VERTICAL;
  f.sut.transmit_state();

  Fixture g;
  EXPECT_TRUE(g.sut.on_receive(Fixture::rx_of(f.transmitter.last)));
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
