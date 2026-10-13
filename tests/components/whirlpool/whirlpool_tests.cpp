#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/whirlpool/whirlpool.h"
#include "esphome/core/preferences.h"

namespace esphome::whirlpool::testing {

static constexpr size_t FRAME_LEN = 21;
using Frame = std::array<uint8_t, FRAME_LEN>;

static constexpr int32_t HEADER_MARK = 9000;
static constexpr int32_t HEADER_SPACE = 4494;
static constexpr int32_t BIT_MARK = 572;
static constexpr int32_t ONE_SPACE = 1659;
static constexpr int32_t ZERO_SPACE = 553;
static constexpr int32_t GAP_SPACE = 7960;

static bool divider_after(size_t bytes_sent) { return bytes_sent == 6 || bytes_sent == 14; }

/// The waveform for a frame: header, bytes LSB first, a divider after bytes 6 and 14, final mark
static remote_base::RawTimings waveform(const Frame &bytes) {
  remote_base::RawTimings raw;
  raw.push_back(HEADER_MARK);
  raw.push_back(-HEADER_SPACE);
  for (size_t i = 0; i < FRAME_LEN; i++) {
    for (int bit = 0; bit < 8; bit++) {
      raw.push_back(BIT_MARK);
      raw.push_back((bytes[i] >> bit) & 1 ? -ONE_SPACE : -ZERO_SPACE);
    }
    if (divider_after(i + 1)) {
      raw.push_back(BIT_MARK);
      raw.push_back(-GAP_SPACE);
    }
  }
  raw.push_back(BIT_MARK);
  return raw;
}

/// Decodes a captured waveform back into bytes, checking header, dividers and footer positions
static ::testing::AssertionResult decode(const remote_base::RawTimings &raw, Frame &out) {
  static constexpr size_t EXPECTED_LEN = 2 + FRAME_LEN * 16 + 2 * 2 + 1;
  if (raw.size() != EXPECTED_LEN)
    return ::testing::AssertionFailure() << "timing count " << raw.size() << ", expected " << EXPECTED_LEN;
  if (raw[0] != HEADER_MARK || raw[1] != -HEADER_SPACE)
    return ::testing::AssertionFailure() << "bad header " << raw[0] << " " << raw[1];
  size_t pos = 2;
  for (size_t i = 0; i < FRAME_LEN; i++) {
    uint8_t value = 0;
    for (int bit = 0; bit < 8; bit++) {
      if (raw[pos] != BIT_MARK)
        return ::testing::AssertionFailure() << "byte " << i << " bit " << bit << " mark " << raw[pos];
      if (raw[pos + 1] == -ONE_SPACE) {
        value |= 1 << bit;
      } else if (raw[pos + 1] != -ZERO_SPACE) {
        return ::testing::AssertionFailure() << "byte " << i << " bit " << bit << " space " << raw[pos + 1];
      }
      pos += 2;
    }
    out[i] = value;
    if (divider_after(i + 1)) {
      if (raw[pos] != BIT_MARK || raw[pos + 1] != -GAP_SPACE)
        return ::testing::AssertionFailure() << "missing divider after byte " << i + 1;
      pos += 2;
    }
  }
  if (raw[pos] != BIT_MARK)
    return ::testing::AssertionFailure() << "bad footer " << raw[pos];
  return ::testing::AssertionSuccess();
}

static uint8_t xor_range(const Frame &f, size_t from, size_t to) {
  uint8_t x = 0;
  for (size_t i = from; i < to; i++)
    x ^= f[i];
  return x;
}

static ::testing::AssertionResult checksums_ok(const Frame &f) {
  if (xor_range(f, 2, 13) != f[13])
    return ::testing::AssertionFailure() << "byte 13 checksum";
  if (xor_range(f, 14, 20) != f[20])
    return ::testing::AssertionFailure() << "byte 20 checksum";
  return ::testing::AssertionSuccess();
}

/// Fills in both checksums of a frame
static Frame with_checksums(Frame f) {
  f[13] = xor_range(f, 2, 13);
  f[20] = xor_range(f, 14, 20);
  return f;
}

static std::string hex(const Frame &f) {
  std::string out;
  for (uint8_t b : f) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X ", b);
    out += buf;
  }
  return out;
}

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
  }
};

struct Fixture {
  CapturingTransmitter transmitter;
  WhirlpoolClimate climate;

  explicit Fixture(Model model) {
    // Every fixture shares one preference key, so clear what an earlier test saved or setup restores it
    if (global_preferences != nullptr)
      global_preferences->reset();
    this->climate.set_model(model);
    this->climate.set_transmitter(&this->transmitter);
    this->climate.setup();
  }

  void command(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan,
               optional<climate::ClimateSwingMode> swing = {}) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_fan_mode(fan);
    if (swing.has_value()) {
      call.set_swing_mode(*swing);
    }
    call.perform();
  }

  /// Decodes the last transmitted frame, failing the test if it is malformed or has bad checksums
  Frame last_frame() const {
    Frame f{};
    EXPECT_FALSE(this->transmitter.frames.empty());
    if (this->transmitter.frames.empty())
      return f;
    EXPECT_TRUE(decode(this->transmitter.frames.back(), f));
    EXPECT_TRUE(checksums_ok(f)) << hex(f);
    return f;
  }
};

static void expect_frame(const Frame &actual, const Frame &expected) {
  EXPECT_EQ(actual, expected) << "actual   " << hex(actual) << "\nexpected " << hex(expected);
}

// The receive path ignores frames for 500 ms after a transmit, so these run on a fixture that has not transmitted
static bool receive(WhirlpoolClimate &climate, const Frame &bytes) {
  const remote_base::RawTimings raw = waveform(bytes);
  remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  return static_cast<remote_base::RemoteReceiverListener &>(climate).on_receive(data);
}

TEST(WhirlpoolDG11J13A, CoolFromOffMatchesKnownFrame) {
  Fixture f(MODEL_DG11J1_3A);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW);

  // byte 2: power toggle 0x04 | fan low 3; byte 3: (24 - 18) << 4 | cool 2; byte 15: cool overwrites toggle with 6
  expect_frame(f.last_frame(), {0x83, 0x06, 0x07, 0x62, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0xE5, 0x00, 0x06, 0x00, 0x00, 0x08, 0x00, 0x0E});
}

TEST(WhirlpoolDG11J191, CoolFromOffUsesSixteenDegreeBase) {
  Fixture f(MODEL_DG11J1_91);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW);

  // byte 3: (24 - 16) << 4 | cool 2
  expect_frame(f.last_frame(), {0x83, 0x06, 0x07, 0x82, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x08, 0x00, 0x0E});
}

TEST(WhirlpoolHisenseArg, PowerToggleThenSettingsOnly) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_OFF);
  // byte 11 display on, byte 12 ambient 24 C, byte 15 power toggle
  expect_frame(f.last_frame(), {0x83, 0x06, 0x07, 0x82, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
                                0x80, 0x18, 0x9D, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x09});

  // Only the fan changed: no power bit, byte 15 settings only
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_MEDIUM);
  expect_frame(f.last_frame(), {0x83, 0x06, 0x02, 0x82, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
                                0x80, 0x18, 0x98, 0x00, 0x02, 0x00, 0x00, 0x08, 0x00, 0x0A});
}

TEST(WhirlpoolHisenseArg, ModeSpecificFanAndTemperature) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW);

  f.command(climate::CLIMATE_MODE_DRY, 24, climate::CLIMATE_FAN_HIGH);
  Frame frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x03, 0) << "dry forces fan auto";
  EXPECT_EQ(frame[3], 0x83);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_HIGH) << "the user's choice survives the dry frame";

  f.command(climate::CLIMATE_MODE_FAN_ONLY, 20, climate::CLIMATE_FAN_AUTO);
  frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x03, 2) << "fan-only with fan auto sends medium";
  EXPECT_EQ(frame[3], 0x74) << "fixed temperature nibble 7, fan mode 4";

  f.command(climate::CLIMATE_MODE_FAN_ONLY, 20, climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(f.last_frame()[2] & 0x03, 1);

  struct RangeCase {
    float target;
    uint8_t range;
  };
  // 23 C (step 7) shares the catch-all code with the high end of the range
  for (const RangeCase &c :
       {RangeCase{16, 0x30}, RangeCase{22, 0x50}, RangeCase{23, 0x20}, RangeCase{24, 0x40}, RangeCase{30, 0x20}}) {
    f.command(climate::CLIMATE_MODE_HEAT_COOL, c.target, climate::CLIMATE_FAN_LOW);
    frame = f.last_frame();
    EXPECT_EQ(frame[3], 0x71) << "target " << c.target;
    EXPECT_EQ(frame[2] & 0x70, c.range) << "target " << c.target;
    EXPECT_EQ(frame[2] & 0x03, 3) << "target " << c.target;
    EXPECT_EQ(frame[15], 0x02) << "target " << c.target;
  }
}

TEST(WhirlpoolHisenseArg, SwingIsSentOnEveryTransmit) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_VERTICAL);
  Frame frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0x80);
  EXPECT_EQ(frame[8], 0x40);

  // A later command that does not mention swing still carries it
  f.command(climate::CLIMATE_MODE_COOL, 25, climate::CLIMATE_FAN_LOW);
  frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0x80);
  EXPECT_EQ(frame[8], 0x40);

  f.command(climate::CLIMATE_MODE_COOL, 25, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_OFF);
  frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0);
  EXPECT_EQ(frame[8], 0x00);

  f.command(climate::CLIMATE_MODE_COOL, 26, climate::CLIMATE_FAN_LOW);
  frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0);
  EXPECT_EQ(frame[8], 0x00);
}

TEST(WhirlpoolDG11J13A, SwingIsSentOnlyWhenItChanges) {
  Fixture f(MODEL_DG11J1_3A);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_VERTICAL);
  Frame frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0x80);
  EXPECT_EQ(frame[8], 0x40);

  f.command(climate::CLIMATE_MODE_COOL, 25, climate::CLIMATE_FAN_LOW);
  frame = f.last_frame();
  EXPECT_EQ(frame[2] & 0x80, 0);
  EXPECT_EQ(frame[8], 0x00);
}

// Cool 24 C (step 8), fan low, powered on assumed
static Frame hisense_cool_frame() {
  return with_checksums({0x83, 0x06, 0x03, 0x82, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
                         0x80, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00, 0x08, 0x00, 0x00});
}

static void assume_on(WhirlpoolClimate &climate, climate::ClimateMode mode, float target) {
  climate.mode = mode;
  climate.target_temperature = target;
  climate.powered_on_assumed = true;
}

TEST(WhirlpoolHisenseArg, ReceiveSwingIsAbsolute) {
  Fixture f(MODEL_HISENSE_ARG);
  assume_on(f.climate, climate::CLIMATE_MODE_COOL, 20);

  Frame frame = hisense_cool_frame();
  frame[8] = 0x40;
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  // The same frame again keeps vertical; the DG11J1 path would toggle back
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);

  frame[8] = 0x00;
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(WhirlpoolHisenseArg, ReceiveTemperature) {
  Fixture f(MODEL_HISENSE_ARG);
  assume_on(f.climate, climate::CLIMATE_MODE_COOL, 20);

  EXPECT_TRUE(receive(f.climate, hisense_cool_frame()));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);

  // Heat-cool carries a fixed temperature nibble, which must not overwrite the target
  Frame frame = hisense_cool_frame();
  frame[2] = 0x43;
  frame[3] = 0x71;
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);

  // Fan-only likewise
  frame[2] = 0x02;
  frame[3] = 0x74;
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
}

TEST(WhirlpoolHisenseArg, ReceivePowerBitTogglesAssumption) {
  Fixture f(MODEL_HISENSE_ARG);
  ASSERT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
  ASSERT_FALSE(f.climate.powered_on_assumed);

  Frame frame = hisense_cool_frame();
  frame[2] |= 0x04;
  frame[15] = 0x01;
  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_TRUE(f.climate.powered_on_assumed);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);

  EXPECT_TRUE(receive(f.climate, with_checksums(frame)));
  EXPECT_FALSE(f.climate.powered_on_assumed);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
}

TEST(WhirlpoolHisenseArg, ReceiveRejectsBadChecksum) {
  Fixture f(MODEL_HISENSE_ARG);
  assume_on(f.climate, climate::CLIMATE_MODE_COOL, 20);

  Frame frame = hisense_cool_frame();
  frame[13] ^= 0x01;
  EXPECT_FALSE(receive(f.climate, frame));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20);

  frame = hisense_cool_frame();
  frame[20] ^= 0x01;
  EXPECT_FALSE(receive(f.climate, frame));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20);
}

TEST(WhirlpoolHisenseArg, ReceiveIgnoredRightAfterTransmit) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_COOL, 20, climate::CLIMATE_FAN_LOW);
  EXPECT_FALSE(receive(f.climate, hisense_cool_frame()));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20);
}

TEST(WhirlpoolHisenseArg, PowerOffKeepsTheCoolModeCode) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  f.command(climate::CLIMATE_MODE_OFF, 24, climate::CLIMATE_FAN_LOW);
  const Frame frame = f.last_frame();
  // The hardware-tested remote sends the cool code with the power toggle, not the heat code 0
  EXPECT_EQ(frame[3] & 0x07, 2);
  EXPECT_EQ(frame[2] & 0x04, 0x04);
  EXPECT_EQ(frame[15], 0x01);
}

TEST(WhirlpoolHisenseArg, EveryTransmitHasValidChecksums) {
  Fixture f(MODEL_HISENSE_ARG);
  f.command(climate::CLIMATE_MODE_HEAT, 30, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL);
  f.command(climate::CLIMATE_MODE_HEAT_COOL, 22, climate::CLIMATE_FAN_AUTO);
  f.command(climate::CLIMATE_MODE_DRY, 18, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF);
  f.command(climate::CLIMATE_MODE_FAN_ONLY, 25, climate::CLIMATE_FAN_LOW);
  f.command(climate::CLIMATE_MODE_OFF, 25, climate::CLIMATE_FAN_LOW);
  ASSERT_EQ(f.transmitter.frames.size(), 5u);
  for (const auto &raw : f.transmitter.frames) {
    Frame frame{};
    ASSERT_TRUE(decode(raw, frame));
    EXPECT_TRUE(checksums_ok(frame)) << hex(frame);
    EXPECT_EQ(frame[0], 0x83);
    EXPECT_EQ(frame[1], 0x06);
    EXPECT_EQ(frame[11], 0x80);
    EXPECT_EQ(frame[12], 0x18);
  }
}

}  // namespace esphome::whirlpool::testing
