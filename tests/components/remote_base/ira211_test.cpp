#include <gtest/gtest.h>
#include "esphome/components/remote_base/ira211_protocol.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace esphome::remote_base::testing {

namespace {

constexpr uint32_t T_US = 800;
constexpr uint32_t HEADER_US = 7600;
constexpr size_t PREAMBLE_ITEMS = 4;  // 7600 mark, 800 space, 800 mark, 7600 space
constexpr size_t BITS_PER_PACKET = 10;

constexpr std::array<IRA211Command, 6> ALL_COMMANDS = {
    IRA211Command::IRA211_COMMAND_TEMP_UP, IRA211Command::IRA211_COMMAND_TEMP_DOWN, IRA211Command::IRA211_COMMAND_MODE,
    IRA211Command::IRA211_COMMAND_FAN,     IRA211Command::IRA211_COMMAND_POWER,     IRA211Command::IRA211_COMMAND_SYNC,
};
constexpr std::array<IRA211Mode, 3> ALL_MODES = {
    IRA211Mode::IRA211_MODE_PROTECTION,
    IRA211Mode::IRA211_MODE_TIMER,
    IRA211Mode::IRA211_MODE_COMFORT,
};
constexpr std::array<IRA211Fan, 4> ALL_FANS = {
    IRA211Fan::IRA211_FAN_AUTO,
    IRA211Fan::IRA211_FAN_LOW,
    IRA211Fan::IRA211_FAN_MEDIUM,
    IRA211Fan::IRA211_FAN_HIGH,
};
constexpr std::array<float, 3> TEMPERATURES = {5.0f, 20.5f, 35.0f};

bool carries_temperature(IRA211Command command) {
  return command != IRA211Command::IRA211_COMMAND_MODE && command != IRA211Command::IRA211_COMMAND_FAN;
}

IRA211Data make_data(IRA211Command command, float temperature, IRA211Mode mode, IRA211Fan fan) {
  IRA211Data data{};
  data.command = command;
  data.set_temperature(temperature);
  data.mode = mode;
  data.fan = fan;
  return data;
}

RawTimings encode(const IRA211Data &data) {
  RemoteTransmitData transmit;
  IRA211Protocol().encode(&transmit, data);
  return transmit.get_data();
}

optional<IRA211Data> decode(const RawTimings &timings) {
  return IRA211Protocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

std::string describe(const IRA211Data &data) {
  char buf[64];
  snprintf(buf, sizeof(buf), "command %d temperature %.1f mode %d fan %d", int(data.command), data.get_temperature(),
           int(data.mode), int(data.fan));
  return buf;
}

void expect_round_trip(const RawTimings &timings, const IRA211Data &expected) {
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value()) << describe(expected);
  // clang-tidy's unchecked-optional-access models neither gtest's ASSERT_TRUE nor value() as a check
  if (decoded.has_value()) {
    EXPECT_TRUE(*decoded == expected) << describe(expected) << " decoded as " << describe(*decoded);
    if (carries_temperature(expected.command))
      EXPECT_EQ(decoded->get_temperature(), expected.get_temperature()) << describe(expected);
  }
}

// Index of the first run of packet `packet`, and of the run after its last one. Packets start with a 1 bit
// after a 0 end bit and end with a 0 bit before a 1 start bit, so no run crosses a packet boundary.
std::pair<size_t, size_t> packet_run_range(const RawTimings &timings, size_t packet) {
  size_t begin = 0, end = 0, bits = 0;
  for (size_t i = PREAMBLE_ITEMS; i < timings.size(); i++) {
    if (bits == packet * BITS_PER_PACKET)
      begin = i;
    if (bits == (packet + 1) * BITS_PER_PACKET) {
      end = i;
      break;
    }
    bits += (std::abs(timings[i]) + T_US / 2) / T_US;
  }
  return {begin, end};
}

}  // namespace

TEST(IRA211ProtocolTest, RoundTripsEveryCommandTemperatureModeAndFan) {
  for (auto command : ALL_COMMANDS) {
    for (auto temperature : TEMPERATURES) {
      for (auto mode : ALL_MODES) {
        for (auto fan : ALL_FANS) {
          const IRA211Data data = make_data(command, temperature, mode, fan);
          expect_round_trip(encode(data), data);
        }
      }
    }
  }
}

TEST(IRA211ProtocolTest, EveryFrameEndsWithTheTrailingMarkAndGap) {
  for (auto command : ALL_COMMANDS) {
    const RawTimings timings =
        encode(make_data(command, 20.5f, IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_AUTO));
    ASSERT_GE(timings.size(), PREAMBLE_ITEMS + 2) << "command " << int(command);
    EXPECT_EQ(timings[timings.size() - 2], int32_t(T_US)) << "command " << int(command);
    EXPECT_EQ(timings.back(), -int32_t(HEADER_US)) << "command " << int(command);
  }
}

TEST(IRA211ProtocolTest, SetTemperatureClampsAndRoundsToHalfDegrees) {
  const struct {
    float input;
    float expected;
  } cases[] = {
      {4.0f, 5.0f},  {36.0f, 35.0f},  {22.26f, 22.5f}, {22.24f, 22.0f},
      {-1.0f, 5.0f}, {140.0f, 35.0f}, {5.0f, 5.0f},    {35.0f, 35.0f},
  };
  for (const auto &c : cases) {
    IRA211Data data{};
    data.set_temperature(c.input);
    EXPECT_EQ(data.get_temperature(), c.expected) << "input " << c.input;
  }
}

// A receiver that stops capturing at the frame gap never sees the trailing mark nor the gap after it.
TEST(IRA211ProtocolTest, DecodesWithoutTheTrailingMarkAndGap) {
  for (auto command : ALL_COMMANDS) {
    for (auto fan : ALL_FANS) {
      const IRA211Data data = make_data(command, 35.0f, IRA211Mode::IRA211_MODE_TIMER, fan);
      RawTimings timings = encode(data);
      timings.pop_back();  // 7600 us gap
      timings.pop_back();  // 800 us trailing mark
      expect_round_trip(timings, data);
    }
  }
}

// Move one bit between two neighbouring runs inside the checksum packet: the bit count and the framing bits
// stay valid, so only the checksum byte changes.
TEST(IRA211ProtocolTest, RejectsACorruptedChecksum) {
  for (auto command : ALL_COMMANDS) {
    const IRA211Data data = make_data(command, 20.5f, IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_LOW);
    RawTimings timings = encode(data);
    ASSERT_TRUE(decode(timings).has_value()) << describe(data);
    size_t bits = 0;
    for (size_t i = PREAMBLE_ITEMS; i + 2 < timings.size(); i++)
      bits += (std::abs(timings[i]) + T_US / 2) / T_US;
    ASSERT_EQ(bits % BITS_PER_PACKET, 0u) << describe(data);
    const auto [begin, end] = packet_run_range(timings, bits / BITS_PER_PACKET - 1);
    ASSERT_GT(end, begin) << describe(data);
    // Shrink the first run that is at least two bits long and grow its neighbour; a run that keeps one bit
    // keeps the start or end bit it may hold.
    bool corrupted = false;
    for (size_t i = begin; i < end && !corrupted; i++) {
      if (std::abs(timings[i]) < int32_t(2 * T_US))
        continue;
      const size_t neighbour = i + 1 < end ? i + 1 : i - 1;
      timings[i] -= timings[i] > 0 ? int32_t(T_US) : -int32_t(T_US);
      timings[neighbour] += timings[neighbour] > 0 ? int32_t(T_US) : -int32_t(T_US);
      corrupted = true;
    }
    ASSERT_TRUE(corrupted) << describe(data) << ": checksum packet has no run longer than one bit";
    EXPECT_FALSE(decode(timings).has_value()) << describe(data);
  }
}

// A SYNC frame carries seven packets; dropping a middle one leaves a well formed six packet frame whose
// command still claims seven, so it must be rejected.
TEST(IRA211ProtocolTest, RejectsAFrameOfTheWrongLengthForItsCommand) {
  const IRA211Data data =
      make_data(IRA211Command::IRA211_COMMAND_SYNC, 20.5f, IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_HIGH);
  RawTimings timings = encode(data);
  ASSERT_TRUE(decode(timings).has_value());
  const auto [begin, end] = packet_run_range(timings, 3);  // the tenths byte
  ASSERT_GT(begin, PREAMBLE_ITEMS - 1);
  ASSERT_GT(end, begin);
  timings.erase(timings.begin() + begin, timings.begin() + end);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(IRA211ProtocolTest, RejectsAForeignFrame) {
  const RawTimings nec_like = {9000, -4500, 560, -560, 560, -1690, 560, -560, 560};
  EXPECT_FALSE(decode(nec_like).has_value());
  EXPECT_FALSE(decode({}).has_value());
}

TEST(IRA211ProtocolTest, EqualityIgnoresFieldsTheCommandDoesNotCarry) {
  const IRA211Data temp_up_auto = make_data(IRA211Command::IRA211_COMMAND_TEMP_UP, 20.5f,
                                            IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_AUTO);
  const IRA211Data temp_up_high = make_data(IRA211Command::IRA211_COMMAND_TEMP_UP, 20.5f, IRA211Mode::IRA211_MODE_TIMER,
                                            IRA211Fan::IRA211_FAN_HIGH);
  EXPECT_TRUE(temp_up_auto == temp_up_high);
  const IRA211Data temp_down = make_data(IRA211Command::IRA211_COMMAND_TEMP_DOWN, 20.5f,
                                         IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_AUTO);
  EXPECT_FALSE(temp_up_auto == temp_down);
  const IRA211Data temp_up_colder = make_data(IRA211Command::IRA211_COMMAND_TEMP_UP, 20.0f,
                                              IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_AUTO);
  EXPECT_FALSE(temp_up_auto == temp_up_colder);

  const IRA211Data mode_low =
      make_data(IRA211Command::IRA211_COMMAND_MODE, 5.0f, IRA211Mode::IRA211_MODE_TIMER, IRA211Fan::IRA211_FAN_LOW);
  const IRA211Data mode_high =
      make_data(IRA211Command::IRA211_COMMAND_MODE, 35.0f, IRA211Mode::IRA211_MODE_TIMER, IRA211Fan::IRA211_FAN_HIGH);
  EXPECT_TRUE(mode_low == mode_high);

  const IRA211Data sync_auto =
      make_data(IRA211Command::IRA211_COMMAND_SYNC, 20.5f, IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_AUTO);
  const IRA211Data sync_high =
      make_data(IRA211Command::IRA211_COMMAND_SYNC, 20.5f, IRA211Mode::IRA211_MODE_COMFORT, IRA211Fan::IRA211_FAN_HIGH);
  EXPECT_FALSE(sync_auto == sync_high);
}

}  // namespace esphome::remote_base::testing
