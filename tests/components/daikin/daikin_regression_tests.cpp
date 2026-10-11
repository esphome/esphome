// Regression tests for the behaviour the daikin climate had before presets were added.
// Every expected byte is worked out by hand from the original daikin.cpp, and this file
// only uses the API that both the original and the current component provide.
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <new>
#include <vector>

#include "esphome/components/daikin/daikin.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::daikin::regression {

static constexpr size_t MESSAGE_SIZE = 35;
using Message = std::array<uint8_t, MESSAGE_SIZE>;

static constexpr int32_t HEADER_MARK = 3360;
static constexpr int32_t HEADER_SPACE = 1760;
static constexpr int32_t BIT_MARK = 520;
static constexpr int32_t ONE_SPACE = 1370;
static constexpr int32_t ZERO_SPACE = 360;
static constexpr int32_t MESSAGE_SPACE = 32300;
static constexpr uint32_t CARRIER = 38000;

// Sum of the constant bytes 16..33 of the third frame (11 DA 27 00 00 .. 06 60 00 00 C0 00 00),
// 0x238 truncated to 8 bits. Byte 34 is this plus bytes 21, 22, 24 and 25.
static constexpr uint8_t FRAME3_CONSTANT_SUM = 0x38;

struct Capture {
  remote_base::RawTimings raw;
  uint32_t carrier;
};

class RecordingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  RecordingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<Capture> sent;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->sent.push_back({this->temp_.get_data(), this->temp_.get_carrier_frequency()});
  }
};

static void read_bytes(const remote_base::RawTimings &raw, size_t &pos, uint8_t *out, size_t count) {
  for (size_t byte = 0; byte < count; byte++) {
    uint8_t value = 0;
    for (int bit = 0; bit < 8; bit++) {
      EXPECT_EQ(raw[pos], BIT_MARK) << "bit mark at " << pos;
      const int32_t space = -raw[pos + 1];
      if (space == ONE_SPACE) {
        value |= 1 << bit;
      } else {
        EXPECT_EQ(space, ZERO_SPACE) << "bit space at " << pos + 1;
      }
      pos += 2;
    }
    out[byte] = value;
  }
}

static void expect_pair(const remote_base::RawTimings &raw, size_t &pos, int32_t mark, int32_t space) {
  EXPECT_EQ(raw[pos], mark) << "mark at " << pos;
  EXPECT_EQ(raw[pos + 1], -space) << "space at " << pos + 1;
  pos += 2;
}

/// Decodes the three frames (8, 8 and 19 bytes) and checks every header, bit and gap timing
static Message decode(const remote_base::RawTimings &raw) {
  Message msg{};
  // 3 headers + 3 trailers of two timings each, plus two timings per bit
  constexpr size_t expected_size = 3 * 4 + 2 * 8 * MESSAGE_SIZE;
  EXPECT_EQ(raw.size(), expected_size);
  if (raw.size() != expected_size)
    return msg;
  size_t pos = 0;
  expect_pair(raw, pos, HEADER_MARK, HEADER_SPACE);
  read_bytes(raw, pos, msg.data(), 8);
  expect_pair(raw, pos, BIT_MARK, MESSAGE_SPACE);
  expect_pair(raw, pos, HEADER_MARK, HEADER_SPACE);
  read_bytes(raw, pos, msg.data() + 8, 8);
  expect_pair(raw, pos, BIT_MARK, MESSAGE_SPACE);
  expect_pair(raw, pos, HEADER_MARK, HEADER_SPACE);
  read_bytes(raw, pos, msg.data() + 16, 19);
  expect_pair(raw, pos, BIT_MARK, 0);
  return msg;
}

/// Receiver timings for a 19-byte frame
static remote_base::RawTimings timings_for(const std::vector<uint8_t> &bytes) {
  remote_base::RawTimings raw;
  raw.push_back(HEADER_MARK);
  raw.push_back(-HEADER_SPACE);
  for (uint8_t byte : bytes) {
    for (int bit = 0; bit < 8; bit++) {
      raw.push_back(BIT_MARK);
      raw.push_back(-((byte >> bit) & 1 ? ONE_SPACE : ZERO_SPACE));
    }
  }
  raw.push_back(BIT_MARK);
  raw.push_back(-MESSAGE_SPACE);
  return raw;
}

/// A 19-byte state frame (type 0x00) with its checksum in byte 18
static std::vector<uint8_t> state_frame(uint8_t mode, uint8_t temperature, uint8_t fan, uint8_t swing) {
  std::vector<uint8_t> f{0x11, 0xDA, 0x27, 0x00, 0x00, mode, temperature, 0x00, fan, swing,
                         0x00, 0x06, 0x60, 0x00, 0x00, 0xC0, 0x00,        0x00, 0x00};
  uint8_t total = 0;
  for (size_t i = 0; i < 18; i++)
    total += f[i];
  f[18] = total;
  return f;
}

struct Rig {
  RecordingTransmitter transmitter;
  DaikinClimate climate;

  Rig() {
    // All instances share one preference key; clear it so setup starts from defaults
    if (global_preferences != nullptr)
      global_preferences->reset();
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    static char name[] = "test_daikin_regression";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->climate.set_transmitter(&this->transmitter);
    static_cast<Component &>(this->climate).setup();
  }

  Message send(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan,
               climate::ClimateSwingMode swing) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_fan_mode(fan);
    call.set_swing_mode(swing);
    call.perform();
    EXPECT_FALSE(this->transmitter.sent.empty());
    if (this->transmitter.sent.empty())
      return Message{};
    return decode(this->transmitter.sent.back().raw);
  }

  bool receive(const std::vector<uint8_t> &bytes) {
    const remote_base::RawTimings raw = timings_for(bytes);
    remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(data);
  }
};

/// Checks every byte that the original encoder never changes, and both checksums
static void expect_fixed_bytes(const Message &m) {
  const uint8_t frame1[8] = {0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, 0x00, 0xD7};
  const uint8_t frame2[8] = {0x11, 0xDA, 0x27, 0x00, 0x42, 0x49, 0x05, 0xA2};
  for (size_t i = 0; i < 8; i++) {
    EXPECT_EQ(m[i], frame1[i]) << "byte " << i;
    EXPECT_EQ(m[8 + i], frame2[i]) << "byte " << 8 + i;
  }
  EXPECT_EQ(m[16], 0x11);
  EXPECT_EQ(m[17], 0xDA);
  EXPECT_EQ(m[18], 0x27);
  EXPECT_EQ(m[19], 0x00);
  EXPECT_EQ(m[20], 0x00);
  EXPECT_EQ(m[23], 0x00);
  EXPECT_EQ(m[26], 0x00);
  EXPECT_EQ(m[27], 0x06);
  EXPECT_EQ(m[28], 0x60);
  EXPECT_EQ(m[29], 0x00);
  EXPECT_EQ(m[30], 0x00);
  EXPECT_EQ(m[31], 0xC0);
  EXPECT_EQ(m[32], 0x00);
  EXPECT_EQ(m[33], 0x00);
  // 0x11 + 0xDA + 0x27 + 0xC5 = 0x1D7
  EXPECT_EQ(m[7], 0xD7);
  const uint8_t expected34 = FRAME3_CONSTANT_SUM + m[21] + m[22] + m[24] + m[25];
  EXPECT_EQ(m[34], expected34);
}

struct ModeCase {
  climate::ClimateMode mode;
  uint8_t byte21;
  uint8_t byte22;
  uint8_t byte34;
};

// At 24 degrees, fan auto (0xA0, 0x00), swing off:
//   OFF       0x00, 24 << 1 = 0x30, 0x38 + 0x00 + 0x30 + 0xA0 = 0x108 -> 0x08
//   COOL      0x31, 0x30,           0x38 + 0x31 + 0x30 + 0xA0 = 0x139 -> 0x39
//   HEAT      0x41, 0x30,           0x38 + 0x41 + 0x30 + 0xA0 = 0x149 -> 0x49
//   DRY       0x21, 0xC0,           0x38 + 0x21 + 0xC0 + 0xA0 = 0x1B9 -> 0xB9
//   FAN_ONLY  0x61, 0x32,           0x38 + 0x61 + 0x32 + 0xA0 = 0x16B -> 0x6B
//   HEAT_COOL 0x01, 0x30,           0x38 + 0x01 + 0x30 + 0xA0 = 0x109 -> 0x09
static const ModeCase MODE_CASES[] = {
    {climate::CLIMATE_MODE_OFF, 0x00, 0x30, 0x08},      {climate::CLIMATE_MODE_COOL, 0x31, 0x30, 0x39},
    {climate::CLIMATE_MODE_HEAT, 0x41, 0x30, 0x49},     {climate::CLIMATE_MODE_DRY, 0x21, 0xC0, 0xB9},
    {climate::CLIMATE_MODE_FAN_ONLY, 0x61, 0x32, 0x6B}, {climate::CLIMATE_MODE_HEAT_COOL, 0x01, 0x30, 0x09},
};

TEST(DaikinRegression, EncodesEveryMode) {
  for (const auto &c : MODE_CASES) {
    Rig r;
    const Message m = r.send(c.mode, 24.0f, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
    SCOPED_TRACE(static_cast<int>(c.mode));
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], c.byte21);
    EXPECT_EQ(m[22], c.byte22);
    EXPECT_EQ(m[24], 0xA0);
    EXPECT_EQ(m[25], 0x00);
    EXPECT_EQ(m[34], c.byte34);
  }
}

struct FanCase {
  climate::ClimateFanMode fan;
  uint8_t b24;
  uint8_t byte34;
};

// Cool at 24 (0x31, 0x30), swing off: b34 = 0x38 + 0x31 + 0x30 + b24 = 0x99 + b24
static const FanCase FAN_CASES[] = {
    {climate::CLIMATE_FAN_QUIET, 0xB0, 0x49},   // 0x99 + 0xB0 = 0x149
    {climate::CLIMATE_FAN_AUTO, 0xA0, 0x39},    // 0x99 + 0xA0 = 0x139
    {climate::CLIMATE_FAN_LOW, 0x30, 0xC9},     // 0x99 + 0x30
    {climate::CLIMATE_FAN_MEDIUM, 0x50, 0xE9},  // 0x99 + 0x50
    {climate::CLIMATE_FAN_HIGH, 0x70, 0x09},    // 0x99 + 0x70 = 0x109
};

TEST(DaikinRegression, EncodesEveryFanMode) {
  for (const auto &c : FAN_CASES) {
    Rig r;
    const Message m = r.send(climate::CLIMATE_MODE_COOL, 24.0f, c.fan, climate::CLIMATE_SWING_OFF);
    SCOPED_TRACE(static_cast<int>(c.fan));
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], 0x31);
    EXPECT_EQ(m[22], 0x30);
    EXPECT_EQ(m[24], c.b24);
    EXPECT_EQ(m[25], 0x00);
    EXPECT_EQ(m[34], c.byte34);
  }
}

struct SwingCase {
  climate::ClimateSwingMode swing;
  uint8_t b24;
  uint8_t b25;
  uint8_t byte34;
};

// Heat at 22 (0x41, 0x2C), fan medium (0x50): b34 = 0x38 + 0x41 + 0x2C + b24 + b25 = 0xA5 + ...
static const SwingCase SWING_CASES[] = {
    {climate::CLIMATE_SWING_OFF, 0x50, 0x00, 0xF5},         // 0xA5 + 0x50
    {climate::CLIMATE_SWING_VERTICAL, 0x5F, 0x00, 0x04},    // 0xA5 + 0x5F = 0x104
    {climate::CLIMATE_SWING_HORIZONTAL, 0x50, 0x0F, 0x04},  // 0xA5 + 0x50 + 0x0F = 0x104
    {climate::CLIMATE_SWING_BOTH, 0x5F, 0x0F, 0x13},        // 0xA5 + 0x5F + 0x0F = 0x113
};

TEST(DaikinRegression, EncodesEverySwingMode) {
  for (const auto &c : SWING_CASES) {
    Rig r;
    const Message m = r.send(climate::CLIMATE_MODE_HEAT, 22.0f, climate::CLIMATE_FAN_MEDIUM, c.swing);
    SCOPED_TRACE(static_cast<int>(c.swing));
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], 0x41);
    EXPECT_EQ(m[22], 0x2C);
    EXPECT_EQ(m[24], c.b24);
    EXPECT_EQ(m[25], c.b25);
    EXPECT_EQ(m[34], c.byte34);
  }
}

TEST(DaikinRegression, EncodesSwingWithQuietAndHighFan) {
  {
    // Dry, quiet, vertical: 0x21, 0xC0, 0xB0 | 0x0F = 0xBF; 0x38 + 0x21 + 0xC0 + 0xBF = 0x1D8 -> 0xD8
    Rig r;
    const Message m =
        r.send(climate::CLIMATE_MODE_DRY, 24.0f, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_SWING_VERTICAL);
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], 0x21);
    EXPECT_EQ(m[22], 0xC0);
    EXPECT_EQ(m[24], 0xBF);
    EXPECT_EQ(m[25], 0x00);
    EXPECT_EQ(m[34], 0xD8);
  }
  {
    // Fan only, low, horizontal: 0x61, 0x32, 0x30, 0x0F; 0x38 + 0x61 + 0x32 + 0x30 + 0x0F = 0x10A -> 0x0A
    Rig r;
    const Message m =
        r.send(climate::CLIMATE_MODE_FAN_ONLY, 24.0f, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_HORIZONTAL);
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], 0x61);
    EXPECT_EQ(m[22], 0x32);
    EXPECT_EQ(m[24], 0x30);
    EXPECT_EQ(m[25], 0x0F);
    EXPECT_EQ(m[34], 0x0A);
  }
  {
    // Heat at 30, high, both: 0x41, 0x3C, 0x7F, 0x0F; 0x38 + 0x41 + 0x3C + 0x7F + 0x0F = 0x143 -> 0x43
    Rig r;
    const Message m = r.send(climate::CLIMATE_MODE_HEAT, 30.0f, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_BOTH);
    expect_fixed_bytes(m);
    EXPECT_EQ(m[21], 0x41);
    EXPECT_EQ(m[22], 0x3C);
    EXPECT_EQ(m[24], 0x7F);
    EXPECT_EQ(m[25], 0x0F);
    EXPECT_EQ(m[34], 0x43);
  }
}

struct TemperatureCase {
  float target;
  uint8_t byte22;
};

// Clamped to 10..30, rounded, then doubled. The call layer does not clamp, so 5 and 35 reach the encoder.
static const TemperatureCase TEMPERATURE_CASES[] = {
    {5.0f, 0x14},  {10.0f, 0x14}, {11.0f, 0x16}, {18.0f, 0x24}, {21.0f, 0x2A},
    {24.0f, 0x30}, {29.0f, 0x3A}, {30.0f, 0x3C}, {35.0f, 0x3C},
};

TEST(DaikinRegression, EncodesWholeDegreesWithClamp) {
  for (const auto &c : TEMPERATURE_CASES) {
    for (auto mode : {climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_HEAT_COOL}) {
      Rig r;
      const Message m = r.send(mode, c.target, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
      SCOPED_TRACE(c.target);
      SCOPED_TRACE(static_cast<int>(mode));
      expect_fixed_bytes(m);
      EXPECT_EQ(m[22], c.byte22);
    }
  }
}

TEST(DaikinRegression, HeatCoolAt18Checksum) {
  // 0x01, 18 << 1 = 0x24, medium 0x50: 0x38 + 0x01 + 0x24 + 0x50 = 0xAD
  Rig r;
  const Message m =
      r.send(climate::CLIMATE_MODE_HEAT_COOL, 18.0f, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF);
  expect_fixed_bytes(m);
  EXPECT_EQ(m[21], 0x01);
  EXPECT_EQ(m[22], 0x24);
  EXPECT_EQ(m[24], 0x50);
  EXPECT_EQ(m[34], 0xAD);
}

TEST(DaikinRegression, FrameTimingsAndCarrier) {
  Rig r;
  r.send(climate::CLIMATE_MODE_COOL, 24.0f, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
  ASSERT_EQ(r.transmitter.sent.size(), 1u);
  const Capture &c = r.transmitter.sent.back();
  EXPECT_EQ(c.carrier, CARRIER);
  ASSERT_EQ(c.raw.size(), 572u);
  // Header, then 64 bits of frame 1, its trailer and the 32300 gap, then frame 2's header
  EXPECT_EQ(c.raw[0], 3360);
  EXPECT_EQ(c.raw[1], -1760);
  EXPECT_EQ(c.raw[2 + 128], 520);
  EXPECT_EQ(c.raw[2 + 128 + 1], -32300);
  EXPECT_EQ(c.raw[2 + 128 + 2], 3360);
  EXPECT_EQ(c.raw[2 + 128 + 3], -1760);
  // Frame 2's trailer and gap, then frame 3's header
  EXPECT_EQ(c.raw[134 + 128], 520);
  EXPECT_EQ(c.raw[134 + 128 + 1], -32300);
  EXPECT_EQ(c.raw[134 + 128 + 2], 3360);
  EXPECT_EQ(c.raw[134 + 128 + 3], -1760);
  // Frame 3 ends with a bit mark and a zero space
  EXPECT_EQ(c.raw[570], 520);
  EXPECT_EQ(c.raw[571], 0);
  // Byte 0 is 0x11, LSB first: bit 0 is a one (1370), bit 1 a zero (360)
  EXPECT_EQ(c.raw[2], 520);
  EXPECT_EQ(c.raw[3], -1370);
  EXPECT_EQ(c.raw[4], 520);
  EXPECT_EQ(c.raw[5], -360);
}

struct ReceiveCase {
  float target;
  uint8_t mode_byte;
  uint8_t temperature;
  uint8_t fan_byte;
  uint8_t swing_byte;
  climate::ClimateMode mode;
  climate::ClimateFanMode fan;
  climate::ClimateSwingMode swing;
};

// Byte 5 mode (low bit power), byte 6 temperature doubled, byte 8 fan high nibble and vertical swing
// low nibble, byte 9 horizontal swing low nibble. Fan nibbles: 3 and 4 low, 5 medium, 6 and 7 high,
// A auto, B quiet.
static const ReceiveCase RECEIVE_CASES[] = {
    {24.0f, 0x31, 0x30, 0xA0, 0x00, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF},
    {22.0f, 0x41, 0x2C, 0x3F, 0x00, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_FAN_LOW,
     climate::CLIMATE_SWING_VERTICAL},
    {20.0f, 0x01, 0x28, 0x50, 0x0F, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_FAN_MEDIUM,
     climate::CLIMATE_SWING_HORIZONTAL},
    {10.0f, 0x31, 0x14, 0x7F, 0x0F, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_BOTH},
    {30.0f, 0x41, 0x3C, 0xB0, 0x00, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_SWING_OFF},
    {18.0f, 0x31, 0x24, 0x40, 0x00, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_OFF},
    {18.0f, 0x31, 0x24, 0x60, 0x00, climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF},
};

TEST(DaikinRegression, DecodesStateFrames) {
  for (const auto &c : RECEIVE_CASES) {
    Rig r;
    SCOPED_TRACE(static_cast<int>(c.mode_byte));
    SCOPED_TRACE(static_cast<int>(c.fan_byte));
    EXPECT_TRUE(r.receive(state_frame(c.mode_byte, c.temperature, c.fan_byte, c.swing_byte)));
    EXPECT_EQ(r.climate.mode, c.mode);
    EXPECT_FLOAT_EQ(r.climate.target_temperature, c.target);
    EXPECT_EQ(r.climate.fan_mode, c.fan);
    EXPECT_EQ(r.climate.swing_mode, c.swing);
  }
}

TEST(DaikinRegression, DecodesDryAndFanOnlyWithoutTouchingTemperature) {
  Rig r;
  ASSERT_TRUE(r.receive(state_frame(0x31, 0x30, 0xA0, 0x00)));
  ASSERT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  // Dry carries 0xC0 in the temperature byte; the target stays at 24
  EXPECT_TRUE(r.receive(state_frame(0x21, 0xC0, 0x7F, 0x0F)));
  EXPECT_EQ(r.climate.mode, climate::CLIMATE_MODE_DRY);
  EXPECT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  EXPECT_EQ(r.climate.fan_mode, climate::CLIMATE_FAN_HIGH);
  EXPECT_EQ(r.climate.swing_mode, climate::CLIMATE_SWING_BOTH);
  // Fan only carries 0x32; the target still stays at 24
  EXPECT_TRUE(r.receive(state_frame(0x61, 0x32, 0xBF, 0x00)));
  EXPECT_EQ(r.climate.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  EXPECT_EQ(r.climate.fan_mode, climate::CLIMATE_FAN_QUIET);
  EXPECT_EQ(r.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST(DaikinRegression, DecodesPowerOffAsOff) {
  Rig r;
  ASSERT_TRUE(r.receive(state_frame(0x31, 0x30, 0xA0, 0x00)));
  ASSERT_EQ(r.climate.mode, climate::CLIMATE_MODE_COOL);
  // Cool with the power bit clear, at 26: mode becomes off and the temperature is not applied
  EXPECT_TRUE(r.receive(state_frame(0x30, 0x34, 0x50, 0x00)));
  EXPECT_EQ(r.climate.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  EXPECT_EQ(r.climate.fan_mode, climate::CLIMATE_FAN_MEDIUM);
}

TEST(DaikinRegression, RejectsBadChecksum) {
  Rig r;
  ASSERT_TRUE(r.receive(state_frame(0x31, 0x30, 0xA0, 0x00)));
  auto frame = state_frame(0x41, 0x2C, 0x50, 0x00);
  frame[18] ^= 0x01;
  EXPECT_FALSE(r.receive(frame));
  EXPECT_EQ(r.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  EXPECT_EQ(r.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
}

TEST(DaikinRegression, RejectsWrongHeaderByte) {
  for (size_t index = 0; index < 5; index++) {
    Rig r;
    ASSERT_TRUE(r.receive(state_frame(0x31, 0x30, 0xA0, 0x00)));
    auto frame = state_frame(0x41, 0x2C, 0x50, 0x00);
    frame[index] ^= 0x01;
    // Recompute the checksum so only the header byte is wrong
    uint8_t total = 0;
    for (size_t i = 0; i < 18; i++)
      total += frame[i];
    frame[18] = total;
    SCOPED_TRACE(index);
    EXPECT_FALSE(r.receive(frame));
    EXPECT_EQ(r.climate.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(r.climate.target_temperature, 24.0f);
  }
}

}  // namespace esphome::daikin::regression
