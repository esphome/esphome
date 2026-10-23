#include <gtest/gtest.h>
#include <functional>
#include <vector>

#include "esphome/components/islandaire/islandaire.h"

namespace esphome::islandaire::testing {

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

class TestIslandaire : public IslandaireClimate {
 public:
  using IslandaireClimate::on_receive;
  using IslandaireClimate::setup;
  using IslandaireClimate::traits;
  using IslandaireClimate::transmit_state;
};

constexpr size_t STATE_LENGTH = 14;
using Frame = std::vector<uint8_t>;

// The timings of the unit's protocol: a header pair, a mark and a short or long space per bit
// (least significant bit first), then a trailing mark
constexpr int32_t HEADER_MARK = 3608;
constexpr int32_t HEADER_SPACE = 1520;
constexpr int32_t BIT_MARK = 560;
constexpr int32_t ONE_SPACE = 1208;
constexpr int32_t ZERO_SPACE = 408;

remote_base::RawTimings encode(const Frame &frame) {
  remote_base::RawTimings timings{HEADER_MARK, -HEADER_SPACE};
  for (uint8_t byte : frame) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      timings.push_back(BIT_MARK);
      timings.push_back(byte & (1 << bit) ? -ONE_SPACE : -ZERO_SPACE);
    }
  }
  timings.push_back(BIT_MARK);
  return timings;
}

Frame bytes_of(const remote_base::RawTimings &timings) {
  Frame bytes((timings.size() - 3) / 16);
  for (size_t bit = 0; bit < bytes.size() * 8; bit++) {
    if (-timings[3 + 2 * bit] > 800)
      bytes[bit / 8] |= 1 << (bit % 8);
  }
  return bytes;
}

// A frame as the layout in islandaire.cpp describes it, with its checksum
Frame state_frame(bool on, uint8_t mode_code, uint8_t temp_f, uint8_t fan_code) {
  Frame f{0x22, 0x22, 0x22, 0x22, 0x22, static_cast<uint8_t>(on ? 0x24 : 0x20), mode_code, temp_f, fan_code,
          0,    0,    0,    0,    0};
  uint8_t sum = 0;
  for (size_t i = 0; i < STATE_LENGTH - 1; i++)
    sum += f[i];
  f[STATE_LENGTH - 1] = sum;
  return f;
}

constexpr uint8_t MODE_HEAT = 0x01;
constexpr uint8_t MODE_COOL = 0x03;
constexpr uint8_t MODE_FAN_ONLY = 0x04;
constexpr uint8_t FAN_AUTO = 0x00;
constexpr uint8_t FAN_LOW = 0x02;
constexpr uint8_t FAN_HIGH = 0x04;

struct Fixture {
  TestIslandaire sut;
  CapturingTransmitter transmitter;

  Fixture() {
    this->sut.set_transmitter(&this->transmitter);
    this->sut.mode = climate::CLIMATE_MODE_COOL;
    this->sut.target_temperature = 72;
    this->sut.fan_mode = climate::CLIMATE_FAN_AUTO;
  }

  Frame sent() const { return bytes_of(this->transmitter.last); }

  bool receive(const remote_base::RawTimings &timings) {
    return this->sut.on_receive(remote_base::RemoteReceiveData(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
  }
};

TEST(IslandaireTests, TraitsReportFahrenheit) {
  Fixture f;
  auto traits = f.sut.traits();
  EXPECT_EQ(traits.get_temperature_unit(), TemperatureUnit::FAHRENHEIT);
  EXPECT_FLOAT_EQ(traits.get_visual_min_temperature(), 61.0f);
  EXPECT_FLOAT_EQ(traits.get_visual_max_temperature(), 88.0f);
}

TEST(IslandaireTests, StateFrameEncodesModeTemperatureAndFan) {
  struct Case {
    const char *name;
    climate::ClimateMode mode;
    climate::ClimateFanMode fan;
    Frame expected;
  };
  const Case cases[] = {
      {"cool auto", climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO, state_frame(true, MODE_COOL, 72, FAN_AUTO)},
      {"heat low", climate::CLIMATE_MODE_HEAT, climate::CLIMATE_FAN_LOW, state_frame(true, MODE_HEAT, 72, FAN_LOW)},
      {"fan high", climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_FAN_HIGH,
       state_frame(true, MODE_FAN_ONLY, 72, FAN_HIGH)},
      {"off", climate::CLIMATE_MODE_OFF, climate::CLIMATE_FAN_AUTO, state_frame(false, MODE_COOL, 72, FAN_AUTO)},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(c.name);
    Fixture f;
    f.sut.mode = c.mode;
    f.sut.fan_mode = c.fan;
    f.sut.transmit_state();
    EXPECT_EQ(f.transmitter.sends, 1);
    EXPECT_EQ(f.sent(), c.expected);
  }
}

TEST(IslandaireTests, TargetIsRoundedAndClampedAndStored) {
  struct Case {
    float target;
    uint8_t sent;
  };
  for (const Case c : {Case{72.6f, 73}, Case{72.4f, 72}, Case{95.0f, 88}, Case{50.0f, 61}}) {
    SCOPED_TRACE(c.target);
    Fixture f;
    f.sut.target_temperature = c.target;
    f.sut.transmit_state();
    EXPECT_EQ(f.sent(), state_frame(true, MODE_COOL, c.sent, FAN_AUTO));
    EXPECT_FLOAT_EQ(f.sut.target_temperature, c.sent);
  }
}

TEST(IslandaireTests, ReceivesStateFrames) {
  Fixture f;
  EXPECT_TRUE(f.receive(encode(state_frame(true, MODE_HEAT, 68, FAN_LOW))));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 68.0f);
  EXPECT_EQ(f.sut.fan_mode, climate::CLIMATE_FAN_LOW);

  // The timer flag in the fan byte and the alternate low code are both read as low
  EXPECT_TRUE(f.receive(encode(state_frame(true, MODE_FAN_ONLY, 80, 0x01 | 0x40))));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 80.0f);
  EXPECT_EQ(f.sut.fan_mode, climate::CLIMATE_FAN_LOW);

  EXPECT_TRUE(f.receive(encode(state_frame(false, MODE_COOL, 72, FAN_HIGH))));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(f.sut.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(IslandaireTests, IgnoresTemperaturesOutsideTheRangeButKeepsTheRest) {
  Fixture f;
  EXPECT_TRUE(f.receive(encode(state_frame(true, MODE_HEAT, 95, FAN_AUTO))));
  EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);
}

TEST(IslandaireTests, RejectsBadFramesWithoutChangingState) {
  struct Case {
    const char *name;
    std::function<void(remote_base::RawTimings &)> corrupt;
  };
  const Case cases[] = {
      {"bad header", [](remote_base::RawTimings &t) { t[0] = 9000; }},
      {"bad preamble",
       [](remote_base::RawTimings &t) { t = encode(state_frame(true, MODE_HEAT, 68, FAN_LOW)), t[3] = -ONE_SPACE; }},
      {"bad checksum", [](remote_base::RawTimings &t) { t[t.size() - 2] = -ONE_SPACE; }},
      {"truncated", [](remote_base::RawTimings &t) { t.resize(t.size() - 20); }},
      {"unknown mode", [](remote_base::RawTimings &t) { t = encode(state_frame(true, 0x02, 68, FAN_LOW)); }},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(c.name);
    Fixture f;
    auto timings = encode(state_frame(true, MODE_HEAT, 68, FAN_LOW));
    c.corrupt(timings);
    EXPECT_FALSE(f.receive(timings));
    EXPECT_EQ(f.sut.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);
    EXPECT_EQ(f.sut.fan_mode, climate::CLIMATE_FAN_AUTO);
  }
}

TEST(IslandaireTests, OwnTransmissionIsAccepted) {
  Fixture f;
  f.sut.mode = climate::CLIMATE_MODE_HEAT;
  f.sut.target_temperature = 65;
  f.sut.fan_mode = climate::CLIMATE_FAN_HIGH;
  f.sut.transmit_state();

  Fixture g;
  EXPECT_TRUE(g.receive(f.transmitter.last));
  EXPECT_EQ(g.sut.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(g.sut.target_temperature, 65.0f);
  EXPECT_EQ(g.sut.fan_mode, climate::CLIMATE_FAN_HIGH);
}

TEST(IslandaireTests, SetupDefaultsTheTargetToSeventyTwoFahrenheit) {
  // Without a restored state the base class picks a Celsius default, which is below the range
  Fixture f;
  f.sut.setup();
  EXPECT_FLOAT_EQ(f.sut.target_temperature, 72.0f);
}

}  // namespace esphome::islandaire::testing
