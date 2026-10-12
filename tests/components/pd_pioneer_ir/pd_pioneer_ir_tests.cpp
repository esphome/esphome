#include <gtest/gtest.h>
#include <cstdint>
#include <new>
#include <vector>

#include "esphome/components/pd_pioneer_ir/pd_pioneer_ir.h"
#include "esphome/components/remote_base/pd_pioneer_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::pd_pioneer_ir::testing {

using remote_base::PDPioneerData;
using Frame = std::vector<uint8_t>;

static constexpr uint32_t TOLERANCE_PERCENT = 25;
static constexpr uint8_t FRAME_SIZE = PDPioneerData::FRAME_SIZE;

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
  }
};

/// Fill in the checksum: the sum of the first 13 values, plus 15 for an odd burst
static Frame finalized(Frame frame) {
  uint8_t sum = frame[3] == 0x02 ? 15 : 0;
  for (size_t idx = 0; idx < FRAME_SIZE - 1; idx++)
    sum += frame[idx];
  frame[FRAME_SIZE - 1] = sum;
  return frame;
}

static Frame odd_frame(uint8_t fan5, uint8_t fan6, uint8_t swing) {
  return finalized({0x23, 0xCB, 0x26, 0x02, 0x00, fan5, fan6, swing, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00});
}

static Frame even_frame(uint8_t power, uint8_t mode, uint8_t temp, uint8_t fan8, uint8_t byte12) {
  return finalized({0x23, 0xCB, 0x26, 0x01, 0x00, power, mode, temp, fan8, 0x00, 0x00, 0x00, byte12, 0x00});
}

static remote_base::RawTimings encode(const Frame &frame) {
  remote_base::RemoteTransmitData data;
  remote_base::PDPioneerProtocol().encode(&data, PDPioneerData(frame));
  return data.get_data();
}

/// RemoteReceiveData keeps a reference to the timings, so they must outlive it
static remote_base::RemoteReceiveData rx(const remote_base::RawTimings &raw) {
  return remote_base::RemoteReceiveData(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
}

static Frame decode(const remote_base::RawTimings &raw) {
  const auto decoded = remote_base::PDPioneerProtocol().decode(rx(raw));
  EXPECT_TRUE(decoded.has_value());
  if (!decoded.has_value())
    return {};
  return Frame(decoded->data(), decoded->data() + FRAME_SIZE);
}

struct Fixture {
  CapturingTransmitter transmitter;
  PDPioneerIR climate;

  Fixture() {
    // Every fixture shares one preference key, so clear what an earlier test saved or setup restores it
    if (global_preferences != nullptr)
      global_preferences->reset();
    // The test main does not construct App as generated code does
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    static char name[] = "test_pd_pioneer_ir";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->climate.set_transmitter(&this->transmitter);
    static_cast<Component &>(this->climate).setup();
  }

  void command(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan,
               climate::ClimateSwingMode swing = climate::CLIMATE_SWING_OFF,
               climate::ClimatePreset preset = climate::CLIMATE_PRESET_NONE) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_fan_mode(fan);
    call.set_swing_mode(swing);
    call.set_preset(preset);
    call.perform();
  }

  /// The odd and even bursts of the last command, decoded by the protocol
  std::pair<Frame, Frame> last_pair() const {
    EXPECT_GE(this->transmitter.frames.size(), 2u);
    if (this->transmitter.frames.size() < 2)
      return {};
    const size_t n = this->transmitter.frames.size();
    return {decode(this->transmitter.frames[n - 2]), decode(this->transmitter.frames[n - 1])};
  }

  bool receive(const remote_base::RawTimings &raw) {
    // The receiver calls through the listener interface, where on_receive is public
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(rx(raw));
  }

  bool receive(const Frame &frame) { return this->receive(encode(frame)); }
};

TEST(PDPioneerIR, ProtocolRoundTripsAFrame) {
  const Frame frame = even_frame(0x24, 0x03, 0x07, 0x05, 0x80);
  EXPECT_EQ(decode(encode(frame)), frame);
  const Frame odd = odd_frame(0x40, 0xC0, 0x00);
  EXPECT_EQ(decode(encode(odd)), odd);
}

TEST(PDPioneerIR, ProtocolRejectsBadChecksumAndUnknownBurst) {
  Frame frame = even_frame(0x24, 0x03, 0x07, 0x05, 0x80);
  frame[13] ^= 0x01;
  const remote_base::RawTimings bad = encode(frame);
  EXPECT_FALSE(remote_base::PDPioneerProtocol().decode(rx(bad)).has_value());
  Frame unknown = even_frame(0x24, 0x03, 0x07, 0x05, 0x80);
  unknown[3] = 0x03;
  unknown = finalized(unknown);
  const remote_base::RawTimings other = encode(unknown);
  EXPECT_FALSE(remote_base::PDPioneerProtocol().decode(rx(other)).has_value());
}

TEST(PDPioneerIR, EncodesEachMode) {
  struct Case {
    climate::ClimateMode mode;
    uint8_t code;
  };
  // 24 C: 31.75 - 24 = 7.75, position 7 = 7, bit 2 of position 12 clear
  static constexpr Case CASES[] = {
      {climate::CLIMATE_MODE_COOL, 0x03},
      {climate::CLIMATE_MODE_HEAT, 0x01},
      {climate::CLIMATE_MODE_DRY, 0x02},
      {climate::CLIMATE_MODE_HEAT_COOL, 0x08},
  };
  for (const auto &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.mode));
    Fixture f;
    f.command(c.mode, 24.0f, climate::CLIMATE_FAN_HIGH);
    const auto [odd, even] = f.last_pair();
    EXPECT_EQ(odd, odd_frame(0x40, 0xC0, 0x00));
    EXPECT_EQ(even, even_frame(0x24, c.code, 0x07, 0x05, 0x80));
  }
}

TEST(PDPioneerIR, FanOnlyCarriesNoTemperature) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_FAN_ONLY, 24.0f, climate::CLIMATE_FAN_LOW);
  const auto [odd, even] = f.last_pair();
  EXPECT_EQ(odd, odd_frame(0x60, 0x40, 0x00));
  // Position 7 keeps the template value 0x0B and position 8 follows the fan
  EXPECT_EQ(even, even_frame(0x24, 0x07, 0x0B, 0x02, 0x80));
}

TEST(PDPioneerIR, EncodesOff) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_OFF, 24.0f, climate::CLIMATE_FAN_HIGH);
  const auto [odd, even] = f.last_pair();
  EXPECT_EQ(odd, odd_frame(0x40, 0xC0, 0x00));
  EXPECT_EQ(even, even_frame(0xA0, 0x01, 0x0C, 0x05, 0x80));
}

TEST(PDPioneerIR, EncodesFanModes) {
  struct Case {
    climate::ClimateFanMode fan;
    uint8_t odd5;
    uint8_t odd6;
    uint8_t even8;
  };
  static constexpr Case CASES[] = {
      {climate::CLIMATE_FAN_AUTO, 0x40, 0x20, 0x00},
      {climate::CLIMATE_FAN_LOW, 0x60, 0x40, 0x02},
      {climate::CLIMATE_FAN_MEDIUM, 0x40, 0x60, 0x03},
      {climate::CLIMATE_FAN_HIGH, 0x40, 0xC0, 0x05},
  };
  for (const auto &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.fan));
    Fixture f;
    f.command(climate::CLIMATE_MODE_COOL, 24.0f, c.fan);
    const auto [odd, even] = f.last_pair();
    EXPECT_EQ(odd, odd_frame(c.odd5, c.odd6, 0x00));
    EXPECT_EQ(even, even_frame(0x24, 0x03, 0x07, c.even8, 0x80));
  }
}

TEST(PDPioneerIR, EncodesSwing) {
  struct Case {
    climate::ClimateSwingMode swing;
    uint8_t odd7;
    uint8_t even8;
  };
  // Vertical swing also ORs 0x38 into the fan field of the even burst (0x05 for high)
  static constexpr Case CASES[] = {
      {climate::CLIMATE_SWING_VERTICAL, 0x08, 0x3D},
      {climate::CLIMATE_SWING_HORIZONTAL, 0x90, 0x05},
      {climate::CLIMATE_SWING_BOTH, 0x98, 0x3D},
  };
  for (const auto &c : CASES) {
    SCOPED_TRACE(static_cast<int>(c.swing));
    Fixture f;
    f.command(climate::CLIMATE_MODE_COOL, 24.0f, climate::CLIMATE_FAN_HIGH, c.swing);
    const auto [odd, even] = f.last_pair();
    EXPECT_EQ(odd, odd_frame(0x40, 0xC0, c.odd7));
    EXPECT_EQ(even, even_frame(0x24, 0x03, 0x07, c.even8, 0x80));
  }
}

TEST(PDPioneerIR, SwingAndFanEncodeInAnyOrder) {
  ControlData fan_first;
  fan_first.set_fan_mode(climate::CLIMATE_FAN_LOW);
  fan_first.set_swing_mode(climate::CLIMATE_SWING_BOTH);
  fan_first.finalize();
  ControlData swing_first;
  swing_first.set_swing_mode(climate::CLIMATE_SWING_BOTH);
  swing_first.set_fan_mode(climate::CLIMATE_FAN_LOW);
  swing_first.finalize();
  EXPECT_EQ(swing_first.odd(), fan_first.odd());
  EXPECT_EQ(swing_first.even(), fan_first.even());
  EXPECT_EQ(swing_first.odd()[7], 0x98);
  EXPECT_EQ(swing_first.even()[8], 0x3A);
}

TEST(PDPioneerIR, EncodesTemperatureInHalfDegrees) {
  struct Case {
    float temperature;
    uint8_t byte7;
    uint8_t byte12;
  };
  // 31 - C in whole degrees, plus a bit for the half: 16 -> 15, 24.5 -> 7 with the bit set, 31 -> 0
  static constexpr Case CASES[] = {
      {16.0f, 0x0F, 0x80}, {24.0f, 0x07, 0x80}, {24.5f, 0x07, 0x84},
      {31.0f, 0x00, 0x80}, {10.0f, 0x0F, 0x80}, {35.0f, 0x00, 0x80},
  };
  for (const auto &c : CASES) {
    SCOPED_TRACE(c.temperature);
    Fixture f;
    f.command(climate::CLIMATE_MODE_COOL, c.temperature, climate::CLIMATE_FAN_HIGH);
    const auto [odd, even] = f.last_pair();
    EXPECT_EQ(even, even_frame(0x24, 0x03, c.byte7, 0x05, c.byte12));
  }
}

TEST(PDPioneerIR, EncodesEcoPreset) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 24.0f, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_PRESET_ECO);
  const auto [odd, even] = f.last_pair();
  EXPECT_EQ(even, even_frame(0x25, 0x03, 0x07, 0x05, 0x80));
}

TEST(PDPioneerIR, OffWithEcoPresetStaysOff) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_OFF, 24.0f, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF,
            climate::CLIMATE_PRESET_ECO);
  const auto [odd, even] = f.last_pair();
  EXPECT_EQ(even, even_frame(0xA0, 0x01, 0x0C, 0x05, 0x80));
}

TEST(PDPioneerIR, ReceivesAnOddAndEvenPair) {
  Fixture f;
  // The odd burst alone changes nothing; the even burst completes the command
  EXPECT_FALSE(f.receive(odd_frame(0x60, 0x40, 0x98)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_TRUE(f.receive(even_frame(0x25, 0x01, 0x07, 0x3A, 0x84)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24.5f);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
}

TEST(PDPioneerIR, ReceivesAnEvenBurstWithoutItsOddBurst) {
  Fixture f;
  // Position 8 is the medium fan code 0x03 with the vertical swing mask 0x38
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x03, 0x07, 0x3B, 0x80)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_MEDIUM);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  // Horizontal swing from an earlier pair does not carry over to an even burst whose odd burst was missed
  EXPECT_FALSE(f.receive(odd_frame(0x60, 0x40, 0x98)));
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x03, 0x07, 0x3A, 0x80)));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_BOTH);
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x03, 0x07, 0x00, 0x80)));
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(PDPioneerIR, ReceivedUnknownModeIsIgnored) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  // Mode byte 0x05 belongs to no mode of this remote
  EXPECT_FALSE(f.receive(even_frame(0x24, 0x05, 0x07, 0x05, 0x80)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.0f);
}

TEST(PDPioneerIR, ReceivedTemperatureIsClamped) {
  Fixture f;
  // Position 7 above 0x0F would decode below the 16 C minimum
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x03, 0x1F, 0x05, 0x80)));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 16.0f);
}

TEST(PDPioneerIR, ReceivedAutoModeIsIgnoredWithoutHeatCool) {
  Fixture f;
  f.climate.set_supports_heat_cool(false);
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  EXPECT_FALSE(f.receive(odd_frame(0x60, 0x40, 0x08)));
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x08, 0x07, 0x3A, 0x80)));
  // The mode stays as it was; the rest of the command still applies
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24.0f);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(PDPioneerIR, ReceivedOffAndFanOnlyKeepTheTemperature) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  EXPECT_FALSE(f.receive(odd_frame(0x40, 0xC0, 0x00)));
  EXPECT_TRUE(f.receive(even_frame(0xA0, 0x01, 0x0C, 0x03, 0x80)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.0f);
  EXPECT_FALSE(f.receive(odd_frame(0x40, 0xC0, 0x00)));
  EXPECT_TRUE(f.receive(even_frame(0x24, 0x07, 0x0B, 0x05, 0x80)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.0f);
}

TEST(PDPioneerIR, TransmittedCommandIsReceivedBack) {
  Fixture sender;
  sender.command(climate::CLIMATE_MODE_DRY, 19.0f, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_VERTICAL);
  Fixture receiver;
  const size_t n = sender.transmitter.frames.size();
  EXPECT_FALSE(receiver.receive(sender.transmitter.frames[n - 2]));
  EXPECT_TRUE(receiver.receive(sender.transmitter.frames[n - 1]));
  EXPECT_EQ(receiver.climate.mode, climate::CLIMATE_MODE_DRY);
  EXPECT_FLOAT_EQ(receiver.climate.target_temperature, 19.0f);
  EXPECT_EQ(receiver.climate.fan_mode, climate::CLIMATE_FAN_MEDIUM);
  EXPECT_EQ(receiver.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(PDPioneerIR, AutoFanIsReceivedBack) {
  Fixture sender;
  sender.command(climate::CLIMATE_MODE_COOL, 24.0f, climate::CLIMATE_FAN_AUTO);
  Fixture receiver;
  const size_t n = sender.transmitter.frames.size();
  EXPECT_FALSE(receiver.receive(sender.transmitter.frames[n - 2]));
  EXPECT_TRUE(receiver.receive(sender.transmitter.frames[n - 1]));
  EXPECT_EQ(receiver.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
}

TEST(PDPioneerIR, StaticCodeIsSentWithItsChecksum) {
  // A 13-byte code, filled and finalized as the transmit action does
  static const uint8_t CODE[] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03, 0x07, 0x05, 0x00, 0x00, 0x00, 0x80};
  remote_base::PDPioneerData data(CODE, sizeof(CODE));
  data.finalize();
  remote_base::RemoteTransmitData dst;
  remote_base::PDPioneerProtocol().encode(&dst, data);
  EXPECT_EQ(decode(dst.get_data()), even_frame(0x24, 0x03, 0x07, 0x05, 0x80));
}

}  // namespace esphome::pd_pioneer_ir::testing
