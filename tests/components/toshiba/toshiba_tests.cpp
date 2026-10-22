#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/toshiba/toshiba.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome::toshiba::testing {

struct Timings {
  uint32_t header_mark;
  uint32_t header_space;
  uint32_t bit_mark;
  uint32_t zero_space;
  uint32_t one_space;
  uint32_t gap_space;
  bool gap_after_last;
};

static constexpr Timings GENERIC{4380, 4370, 540, 540, 1620, 5480, true};
static constexpr Timings SEIYA{4630, 4450, 625, 490, 1570, 5830, false};

/// The waveform the component must produce for a frame: two copies, MSB first
static remote_base::RawTimings waveform(const std::vector<uint8_t> &bytes, const Timings &t, int copies = 2) {
  remote_base::RawTimings raw;
  for (int copy = 0; copy < copies; copy++) {
    raw.push_back(t.header_mark);
    raw.push_back(-static_cast<int32_t>(t.header_space));
    for (uint8_t byte : bytes) {
      for (int bit = 7; bit >= 0; bit--) {
        raw.push_back(t.bit_mark);
        raw.push_back(-static_cast<int32_t>((byte >> bit) & 1 ? t.one_space : t.zero_space));
      }
    }
    raw.push_back(t.bit_mark);
    if (copy + 1 < copies || t.gap_after_last) {
      raw.push_back(-static_cast<int32_t>(t.gap_space));
    }
  }
  return raw;
}

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;
  uint32_t send_delay_ms{0};

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    if (this->send_delay_ms != 0)
      delay(this->send_delay_ms);
    this->frames.push_back(this->temp_.get_data());
  }
};

struct Fixture {
  CapturingTransmitter transmitter;
  ToshibaClimate climate;

  explicit Fixture(Model model, bool setup = true) {
    this->climate.set_model(model);
    this->climate.set_transmitter(&this->transmitter);
    if (setup)
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

  const remote_base::RawTimings &last_frame() const { return this->transmitter.frames.back(); }
};

class PowerSelection : public select::Select {
 public:
  PowerSelection() { this->traits.set_options({"100%", "75%", "50%", "Silent 1", "Silent 2"}); }

 protected:
  void control(size_t) override {}
};

/// Decodes the first copy of a frame back into bytes so a mismatch is readable
static std::string frame_bytes(const remote_base::RawTimings &raw, size_t nbytes) {
  std::string out;
  for (size_t byte = 0; byte < nbytes; byte++) {
    unsigned value = 0;
    for (size_t bit = 0; bit < 8; bit++) {
      const size_t space = 3 + 2 * (byte * 8 + bit);
      value = (value << 1) | (space < raw.size() && -raw[space] > 1000 ? 1 : 0);
    }
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X ", value);
    out += buf;
  }
  return out;
}

// Reports the frame bytes instead of gtest's truncated vector dump
static void expect_frame(const remote_base::RawTimings &actual, const remote_base::RawTimings &expected,
                         size_t nbytes) {
  EXPECT_EQ(actual, expected) << "actual   " << frame_bytes(actual, nbytes) << "\nexpected "
                              << frame_bytes(expected, nbytes);
}

TEST(ToshibaSeiya, EncodesCapturedFrames) {
  Fixture f(MODEL_SEIYA);

  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_AUTO);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x70, 0x01, 0x00, 0x21, 0x17, 0x46}, SEIYA), 11);

  f.command(climate::CLIMATE_MODE_OFF, 23, climate::CLIMATE_FAN_AUTO);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x60, 0x07, 0x00, 0x21, 0x17, 0x50}, SEIYA), 11);

  f.command(climate::CLIMATE_MODE_HEAT, 22, climate::CLIMATE_FAN_AUTO);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x50, 0x03, 0x00, 0x21, 0x17, 0x64}, SEIYA), 11);

  // The swing code goes out once, when the swing mode changes
  f.command(climate::CLIMATE_MODE_DRY, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_VERTICAL);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x50, 0x02, 0x00, 0x21, 0x08, 0x7A}, SEIYA), 11);
  f.command(climate::CLIMATE_MODE_DRY, 23, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_VERTICAL);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x60, 0x02, 0x00, 0x21, 0x17, 0x55}, SEIYA), 11);
}

TEST(ToshibaGeneric, WaveformIsUnchangedByTheSharedPath) {
  Fixture f(MODEL_GENERIC);

  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_AUTO);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x01, 0x00, 0x70}, GENERIC), 9);

  f.command(climate::CLIMATE_MODE_HEAT, 20, climate::CLIMATE_FAN_HIGH);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x30, 0xC3, 0x00, 0xF2}, GENERIC), 9);
}

// The RAC-PT1411HWRU sends its main message, then a separate swing packet
// wrapped in packet spaces; both use the generic timings.
static void expect_swing_packet(const remote_base::RawTimings &actual, const std::vector<uint8_t> &bytes) {
  ASSERT_GE(actual.size(), 2u);
  EXPECT_EQ(actual.front(), -10500);
  EXPECT_EQ(actual.back(), -10500);
  const remote_base::RawTimings frame(actual.begin() + 1, actual.end() - 1);
  expect_frame(frame, waveform(bytes, GENERIC), 6);
}

TEST(ToshibaRacPt1411hwru, SendsTheSwingFrameFromFlash) {
  Fixture f(MODEL_RAC_PT1411HWRU_C);

  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_VERTICAL);
  expect_swing_packet(f.last_frame(), {0xB9, 0x46, 0xF5, 0x0A, 0x04, 0xFB});

  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
  expect_swing_packet(f.last_frame(), {0xB9, 0x46, 0xF5, 0x0A, 0x05, 0xFA});
}

// The temperature tables are read on the way out and searched on the way back
// in, so what one fixture sends must decode to the same state in another.
TEST(ToshibaRacPt1411hwru, TemperatureCodesRoundTrip) {
  for (const Model model : {MODEL_RAC_PT1411HWRU_C, MODEL_RAC_PT1411HWRU_F}) {
    for (int temperature = 16; temperature <= 30; temperature++) {
      SCOPED_TRACE(::testing::Message() << "model " << model << " temperature " << temperature);
      Fixture tx(model);
      tx.command(climate::CLIMATE_MODE_COOL, temperature, climate::CLIMATE_FAN_AUTO);
      // The main message is sent before the swing packet
      const auto &main_frame = tx.transmitter.frames[tx.transmitter.frames.size() - 2];

      Fixture rx(model);
      remote_base::RemoteReceiveData data(main_frame, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
      ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(rx.climate).on_receive(data));
      EXPECT_EQ(rx.climate.mode, climate::CLIMATE_MODE_COOL);
      EXPECT_NEAR(rx.climate.target_temperature, temperature, 0.3f);
    }
  }
}

// The RAS-2819T table is read for the code on the way out and searched on the way back in.
TEST(ToshibaRas2819t, TemperatureCodesRoundTrip) {
  for (int temperature = 18; temperature <= 30; temperature++) {
    SCOPED_TRACE(::testing::Message() << "temperature " << temperature);
    Fixture tx(MODEL_RAS_2819T);
    tx.command(climate::CLIMATE_MODE_COOL, temperature, climate::CLIMATE_FAN_AUTO);

    Fixture rx(MODEL_RAS_2819T);
    remote_base::RemoteReceiveData data(tx.last_frame(), 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(rx.climate).on_receive(data));
    EXPECT_EQ(rx.climate.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(rx.climate.target_temperature, temperature);
  }
}

static bool receive(ToshibaClimate &climate, const std::vector<uint8_t> &bytes, const Timings &t) {
  const remote_base::RawTimings raw = waveform(bytes, t);
  remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  // The receiver calls through the listener interface, where on_receive is public
  return static_cast<remote_base::RemoteReceiverListener &>(climate).on_receive(data);
}

TEST(ToshibaSeiya, DecodesStateAndKeepsSwingOnNeutralByte) {
  Fixture f(MODEL_SEIYA);
  f.command(climate::CLIMATE_MODE_COOL, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_VERTICAL);

  // An ordinary update carries the neutral swing byte and must not touch the swing mode
  EXPECT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x50, 0x02, 0x00, 0x21, 0x17, 0x65}, SEIYA));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_DRY);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);

  // A direction code switches the swing mode
  EXPECT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x50, 0x02, 0x00, 0x21, 0x09, 0x7B}, SEIYA));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_HORIZONTAL);

  // Without the feature marker the swing byte is not a Seiya swing command
  EXPECT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x50, 0x02, 0x00, 0x00, 0x01, 0x52}, SEIYA));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_HORIZONTAL);
}

TEST(ToshibaSeiya, RejectsBadLengthByte) {
  Fixture f(MODEL_SEIYA);
  // Length byte 0x01 passes the header checksum but describes a frame shorter than any Toshiba frame
  EXPECT_FALSE(receive(f.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x01, 0x50, 0x02}, SEIYA));
}

static void set_preset(ToshibaClimate &climate, const char *preset) {
  auto call = climate.make_call();
  call.set_preset(preset);
  call.perform();
}

static void set_fan(ToshibaClimate &climate, const char *fan) {
  auto call = climate.make_call();
  call.set_fan_mode(fan);
  call.perform();
}

struct CapturedState {
  std::vector<uint8_t> bytes;
  float temperature;
  climate::ClimateMode mode;
  const char *fan;
  const char *preset;
  size_t power{0};
};

// WH-TA15PE handheld captures, 2026-10-21. Expected bytes are independent of the encoder.
static const CapturedState CAPTURED_STATES[] = {
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x43, 0x00, 0x32}, 24, climate::CLIMATE_MODE_HEAT, "LOW", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x00, 0x12}, 24, climate::CLIMATE_MODE_HEAT, "Low+", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x83, 0x00, 0xF2}, 24, climate::CLIMATE_MODE_HEAT, "MEDIUM", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0xA3, 0x00, 0xD2}, 24, climate::CLIMATE_MODE_HEAT, "Medium+", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0xC3, 0x00, 0xB2}, 24, climate::CLIMATE_MODE_HEAT, "HIGH", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x23, 0x00, 0x52}, 24, climate::CLIMATE_MODE_HEAT, "QUIET", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x03, 0x00, 0x72}, 24, climate::CLIMATE_MODE_HEAT, "AUTO", "NONE"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x03, 0x00, 0x01, 0x7B}, 24, climate::CLIMATE_MODE_HEAT, "AUTO", "BOOST"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x60, 0x03, 0x00, 0x01, 0x6B}, 23, climate::CLIMATE_MODE_HEAT, "AUTO", "BOOST"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x00, 0x03, 0x19}, 24, climate::CLIMATE_MODE_HEAT, "Low+", "ECO"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x43, 0x02, 0x20}, 23, climate::CLIMATE_MODE_HEAT, "LOW", "Fireplace 1"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x03, 0x11}, 24, climate::CLIMATE_MODE_HEAT, "Low+", "Fireplace 2"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x00, 0x04, 0x1E},
     8,
     climate::CLIMATE_MODE_HEAT,
     "Low+",
     "Frost protection"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x40, 0x63, 0x00, 0x04, 0x2E},
     5,
     climate::CLIMATE_MODE_HEAT,
     "Low+",
     "Frost protection"},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0xC0, 0x63, 0x00, 0x04, 0xAE},
     13,
     climate::CLIMATE_MODE_HEAT,
     "Low+",
     "Frost protection"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x40, 0x52}, 24, climate::CLIMATE_MODE_HEAT, "Low+", "NONE", 1},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x63, 0x80, 0x82}, 23, climate::CLIMATE_MODE_HEAT, "Low+", "NONE", 2},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x00, 0x02, 0x18}, 24, climate::CLIMATE_MODE_HEAT, "Low+", "NONE", 3},
    {{0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x60, 0x63, 0x00, 0x0A, 0x00}, 23, climate::CLIMATE_MODE_HEAT, "Low+", "NONE", 4},
    {{0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x70, 0x63, 0x00, 0x21, 0x17, 0x24},
     24,
     climate::CLIMATE_MODE_HEAT,
     "Low+",
     "HADA Care"},
    {{0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x60, 0x63, 0x00, 0x21, 0x17, 0x34},
     23,
     climate::CLIMATE_MODE_HEAT,
     "Low+",
     "HADA Care"},
    {{0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x70, 0x03, 0x00, 0x04, 0x7F, 0x03, 0x00},
     24,
     climate::CLIMATE_MODE_HEAT,
     "AUTO",
     "Sleep 1h"},
    {{0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x70, 0x03, 0x00, 0x0C, 0x7F, 0x03, 0x08},
     24,
     climate::CLIMATE_MODE_HEAT,
     "AUTO",
     "Sleep 3h"},
    {{0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x70, 0x03, 0x00, 0x14, 0x7F, 0x03, 0x10},
     24,
     climate::CLIMATE_MODE_HEAT,
     "AUTO",
     "Sleep 5h"},
    {{0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x70, 0x03, 0x00, 0x24, 0x7F, 0x03, 0x20},
     24,
     climate::CLIMATE_MODE_HEAT,
     "AUTO",
     "Sleep 9h"},
    {{0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x60, 0x03, 0x00, 0x04, 0x7F, 0x03, 0x10},
     23,
     climate::CLIMATE_MODE_HEAT,
     "AUTO",
     "Sleep 1h"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x60, 0x00, 0x31}, 22, climate::CLIMATE_MODE_HEAT_COOL, "Low+", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x00, 0x61, 0x00, 0x60}, 17, climate::CLIMATE_MODE_COOL, "Low+", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x02, 0x00, 0x53}, 22, climate::CLIMATE_MODE_DRY, "AUTO", "NONE"},
    {{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x63, 0x00, 0x02}, 23, climate::CLIMATE_MODE_HEAT, "Low+", "NONE"},
};

TEST(ToshibaWhTa15pe, EncodesCapturedStates) {
  for (const auto &state : CAPTURED_STATES) {
    SCOPED_TRACE(frame_bytes(waveform(state.bytes, GENERIC), state.bytes.size()));
    global_preferences->reset();
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    auto call = f.climate.make_call();
    call.set_mode(state.mode);
    call.set_target_temperature(state.temperature);
    call.set_fan_mode(state.fan);
    call.set_preset(state.preset);
    call.perform();
    if (state.power != 0)
      f.climate.set_power_selection(state.power);
    ASSERT_FALSE(f.transmitter.frames.empty());
    expect_frame(f.last_frame(), waveform(state.bytes, GENERIC), state.bytes.size());
  }
}

TEST(ToshibaWhTa15pe, DecodesCapturedStatesAndPublishesPowerSelection) {
  PowerSelection power;
  for (const auto &state : CAPTURED_STATES) {
    SCOPED_TRACE(frame_bytes(waveform(state.bytes, GENERIC), state.bytes.size()));
    global_preferences->reset();
    Fixture expected(MODEL_RAS_25J2KVSG_ND);
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.climate.set_power_selection_select(&power);
    ASSERT_TRUE(receive(f.climate, state.bytes, GENERIC));
    EXPECT_EQ(f.climate.mode, state.mode);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, state.temperature);
    EXPECT_EQ(power.active_index(), state.power);
    auto call = expected.climate.make_call();
    call.set_fan_mode(state.fan);
    call.set_preset(state.preset);
    call.set_target_temperature(state.temperature);
    call.set_mode(state.mode);
    call.perform();
    EXPECT_EQ(f.climate.fan_mode, expected.climate.fan_mode);
    EXPECT_EQ(f.climate.get_custom_fan_mode(), expected.climate.get_custom_fan_mode());
    EXPECT_EQ(f.climate.preset, expected.climate.preset);
    EXPECT_EQ(f.climate.get_custom_preset(), expected.climate.get_custom_preset());
    EXPECT_TRUE(f.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, ShortCommandsPreserveStateAndSwingCopiesToggleOnce) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_MEDIUM);
  set_fan(f.climate, "Low+");
  set_preset(f.climate, "ECO");
  const auto transmissions = f.transmitter.frames.size();
  const std::vector<uint8_t> swing{0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25};
  const std::vector<uint8_t> fix{0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x00, 0x21};
  {
    LoopBlockingGuard guard{nullptr, nullptr, 1000};
    ASSERT_TRUE(receive(f.climate, swing, GENERIC));
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  }
  {
    LoopBlockingGuard guard{nullptr, nullptr, 1100};
    ASSERT_TRUE(receive(f.climate, swing, GENERIC));
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
    ASSERT_TRUE(receive(f.climate, fix, GENERIC));
  }
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Low+");
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_EQ(f.transmitter.frames.size(), transmissions);
  {
    LoopBlockingGuard guard{nullptr, nullptr, 2000};
    ASSERT_TRUE(receive(f.climate, swing, GENERIC));
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
  }
}

TEST(ToshibaWhTa15pe, DecodesMeasuredSwingBurstIncludingItsRepeatedCopy) {
  // S04, WH-TA15PE, 2026-10-21 20:08:32 +02:00. Original timings, including both footer marks.
  const remote_base::RawTimings raw{
      4460, -4399, 581, -1590, 585, -1581, 582, -1581, 581,  -1581, 586, -502,  582, -506,  581, -1585, 581, -507,
      581,  -507,  581, -512,  580, -507,  581, -512,  580,  -1582, 580, -1595, 581, -511,  581, -1581, 581, -507,
      581,  -507,  581, -502,  581, -511,  581, -511,  555,  -533,  581, -520,  581, -1581, 555, -1621, 585, -1581,
      582,  -1585, 581, -1590, 581, -1582, 581, -1594, 581,  -1582, 581, -507,  581, -506,  555, -533,  581, -1581,
      525,  -576,  582, -510,  582, -519,  581, -511,  581,  -1586, 581, -507,  551, -532,  581, -507,  581, -507,
      555,  -532,  582, -1589, 582, -507,  580, -516,  581,  -507,  581, -506,  582, -1598, 555, -537,  577, -515,
      577,  -1586, 555, -533,  576, -1581, 560, -5617, 4456, -4399, 581, -1581, 556, -1611, 638, -1537, 525, -1651,
      612,  -480,  608, -475,  612, -1551, 616, -476,  612,  -476,  611, -476,  616, -477,  611, -481,  637, -1525,
      612,  -1551, 611, -477,  637, -1533, 613, -475,  612,  -475,  612, -480,  612, -481,  637, -450,  616, -481,
      611,  -481,  611, -1555, 529, -1651, 529, -1646, 612,  -1550, 612, -1559, 581, -1590, 612, -1555, 612, -1550,
      612,  -472,  616, -471,  638, -450,  638, -1524, 612,  -489,  612, -480,  612, -480,  612, -480,  638, -1533,
      555,  -555,  611, -476,  612, -476,  613, -479,  612,  -481,  612, -1554, 612, -476,  638, -445,  612, -476,
      611,  -476,  612, -1555, 616, -471,  612, -481,  612,  -1550, 612, -475,  612, -1551, 612,
  };
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 23, climate::CLIMATE_FAN_LOW);
  remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 23);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_LOW);
}

TEST(ToshibaWhTa15pe, SendsOnlyMotionForSwingAndFix) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  auto call = f.climate.make_call();
  call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  call.perform();
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC), 7);
  f.climate.fix();
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x00, 0x21}, GENERIC, 1), 7);
  call.set_swing_mode(climate::CLIMATE_SWING_OFF);
  call.perform();
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC), 7);
}

static uint32_t frame_duration_ms(const remote_base::RawTimings &raw) {
  uint32_t duration_us = 0;
  for (const int32_t pulse : raw)
    duration_us += static_cast<uint32_t>(std::abs(pulse));
  return (duration_us + 999) / 1000;
}

TEST(ToshibaWhTa15pe, SwingEchoesDoNotReverseStateOrPreventStopping) {
  for (const bool state_command : {false, true}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
    auto swing = f.climate.make_call();
    swing.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (state_command)
      swing.set_target_temperature(23);
    swing.perform();
    const auto echo = f.last_frame();
    const auto transmissions = f.transmitter.frames.size();
    {
      LoopBlockingGuard guard{nullptr, nullptr, 1000};
      remote_base::RemoteReceiveData data(echo, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
      ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
      EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
    }
    // Copies can also be delivered in separate receiver buffers.
    for (const uint32_t now : {1100u, 1200u}) {
      LoopBlockingGuard guard{nullptr, nullptr, now};
      ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC));
      EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
    }
    EXPECT_EQ(f.transmitter.frames.size(), transmissions);
    auto off = f.climate.make_call();
    off.set_swing_mode(climate::CLIMATE_SWING_OFF);
    off.perform();
    ASSERT_EQ(f.transmitter.frames.size(), transmissions + 1);
    expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC), 7);
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
  }
}

TEST(ToshibaWhTa15pe, SwingEchoWindowCoversTimeSpentWaitingForTheTransmitter) {
  for (const bool state_command : {false, true}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
    auto expected = waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC);
    if (state_command) {
      const auto state = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x03, 0x00, 0x62}, GENERIC);
      expected.insert(expected.begin(), state.begin(), state.end());
    }
    f.transmitter.send_delay_ms = frame_duration_ms(expected) + 220;
    auto swing = f.climate.make_call();
    swing.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (state_command)
      swing.set_target_temperature(23);
    swing.perform();
    expect_frame(f.last_frame(), expected, state_command ? 9 : 7);
    {
      LoopBlockingGuard guard{nullptr, nullptr, 1000};
      remote_base::RemoteReceiveData data(f.last_frame(), 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
      ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
    }
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  }
}

TEST(ToshibaWhTa15pe, HandheldSwingWorksAgainAfterTheEchoWindow) {
  for (const bool state_command : {false, true}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
    auto swing = f.climate.make_call();
    swing.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (state_command)
      swing.set_target_temperature(23);
    swing.perform();
    const auto transmissions = f.transmitter.frames.size();
    delay(frame_duration_ms(f.last_frame()) + 220);
    {
      LoopBlockingGuard guard{nullptr, nullptr, 2000};
      ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC));
      EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
    }
    {
      LoopBlockingGuard guard{nullptr, nullptr, 3000};
      ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC));
      EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
    }
    EXPECT_EQ(f.transmitter.frames.size(), transmissions);
  }
}

TEST(ToshibaWhTa15pe, ReceivesEveryCommandInACompleteStateAndMotionBurst) {
  for (const bool final_space : {true, false}) {
    global_preferences->reset();
    Fixture tx(MODEL_RAS_25J2KVSG_ND);
    tx.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF);
    auto call = tx.climate.make_call();
    call.set_target_temperature(23);
    call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    call.perform();
    auto burst = tx.last_frame();
    if (!final_space)
      burst.pop_back();
    global_preferences->reset();
    Fixture rx(MODEL_RAS_25J2KVSG_ND);
    ASSERT_EQ(rx.climate.swing_mode, climate::CLIMATE_SWING_OFF);
    remote_base::RemoteReceiveData data(burst, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(rx.climate).on_receive(data));
    EXPECT_FLOAT_EQ(rx.climate.target_temperature, 23);
    EXPECT_EQ(rx.climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_EQ(rx.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
    EXPECT_TRUE(rx.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, DoesNotApplyAnInvalidFrameAfterAValidFrame) {
  auto burst = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x03, 0x00, 0x62}, GENERIC, 1);
  auto invalid = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x03, 0x00, 0xFF}, GENERIC, 1);
  burst.insert(burst.end(), invalid.begin(), invalid.end());
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  remote_base::RemoteReceiveData data(burst, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 23);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_TRUE(f.transmitter.frames.empty());
}

TEST(ToshibaWhTa15pe, ReceivesDifferentStateFramesInTheSameBuffer) {
  auto burst = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x03, 0x00, 0x62}, GENERIC);
  const auto update = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x61, 0x00, 0x30}, GENERIC);
  burst.insert(burst.end(), update.begin(), update.end());
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  remote_base::RemoteReceiveData data(burst, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
  ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Low+");
  EXPECT_TRUE(f.transmitter.frames.empty());
}

TEST(ToshibaWhTa15pe, FreshNormalTargetIsValidEvenWithAColdSensor) {
  for (const float temperature : {4.0f, 10.0f, 16.0f, 17.0f, 22.0f, 35.0f, NAN}) {
    global_preferences->reset();
    sensor::Sensor sensor;
    sensor.publish_state(temperature);
    CapturingTransmitter transmitter;
    ToshibaClimate climate;
    climate.set_model(MODEL_RAS_25J2KVSG_ND);
    climate.set_transmitter(&transmitter);
    climate.set_sensor(&sensor);
    climate.setup();
    const float expected = std::isnan(temperature) ? 24 : clamp(temperature, 17.0f, 30.0f);
    EXPECT_FLOAT_EQ(climate.target_temperature, expected);
    EXPECT_EQ(climate.preset, climate::CLIMATE_PRESET_NONE);
    EXPECT_FLOAT_EQ(climate.get_traits().get_visual_min_temperature(), 5);
    EXPECT_TRUE(transmitter.frames.empty());

    // FAN ONLY must also use the normal target initialized at startup.
    ASSERT_TRUE(receive(climate, {0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x64, 0x00, 0x35}, GENERIC));
    EXPECT_FLOAT_EQ(climate.target_temperature, expected);
    climate.set_power_selection(1);
    std::vector<uint8_t> bytes{
        0xF2, 0x0D, 0x03, 0xFC, 0x01, static_cast<uint8_t>((static_cast<uint8_t>(expected) - 17) << 4), 0x64, 0x40, 0};
    bytes.back() = xor_bytes(bytes.data() + 4, bytes.size() - 5);
    expect_frame(transmitter.frames.back(), waveform(bytes, GENERIC), 9);
    auto heat = climate.make_call();
    heat.set_mode(climate::CLIMATE_MODE_HEAT);
    heat.perform();
    EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_EQ(transmitter.frames.size(), 2u);
    auto off = climate.make_call();
    off.set_mode(climate::CLIMATE_MODE_OFF);
    off.perform();
    EXPECT_EQ(climate.mode, climate::CLIMATE_MODE_OFF);
    EXPECT_EQ(transmitter.frames.size(), 3u);
  }
}

TEST(ToshibaWhTa15pe, OrdinaryFramesPreserveSwingAndDoNotEchoReceivedCommands) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC));
  ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x63, 0x00, 0x02}, GENERIC));
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  auto call = f.climate.make_call();
  call.set_target_temperature(24);
  call.perform();
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x00, 0x12}, GENERIC), 9);
}

TEST(ToshibaWhTa15pe, FrostProtectionRangeAndNormalTargetRestoration) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  set_fan(f.climate, "Low+");
  set_preset(f.climate, "Frost protection");
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 8);
  static constexpr uint8_t CHECKSUMS[] = {0x2E, 0x3E, 0x0E, 0x1E, 0xEE, 0xFE, 0xCE, 0xDE, 0xAE};
  for (uint8_t temperature = 5; temperature <= 13; temperature++) {
    auto call = f.climate.make_call();
    call.set_target_temperature(temperature);
    call.perform();
    expect_frame(f.last_frame(),
                 waveform({0xF2, 0x0D, 0x04, 0xFB, 0x09, static_cast<uint8_t>((temperature - 1) << 4), 0x63, 0x00, 0x04,
                           CHECKSUMS[temperature - 5]},
                          GENERIC),
                 10);
  }
  const auto transmissions = f.transmitter.frames.size();
  for (float temperature : {4.0f, 14.0f, 16.0f, 17.0f, INFINITY}) {
    auto call = f.climate.make_call();
    call.set_target_temperature(temperature);
    call.perform();
    EXPECT_EQ(f.transmitter.frames.size(), transmissions);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 13);
  }
  set_preset(f.climate, "NONE");
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Low+");
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x00, 0x12}, GENERIC), 9);
  const auto normal_transmissions = f.transmitter.frames.size();
  for (float temperature : {5.0f, 13.0f, 14.0f, 16.0f, 31.0f}) {
    auto call = f.climate.make_call();
    call.set_target_temperature(temperature);
    call.perform();
    EXPECT_EQ(f.transmitter.frames.size(), normal_transmissions);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
  }
}

TEST(ToshibaWhTa15pe, SleepForcesAutoAndCancellationRestoresCustomFan) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  set_fan(f.climate, "Low+");
  for (const char *preset : {"Sleep 1h", "Sleep 3h", "Sleep 5h", "Sleep 9h"}) {
    set_preset(f.climate, preset);
    EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
    EXPECT_FALSE(f.climate.has_custom_fan_mode());
  }
  auto call = f.climate.make_call();
  call.set_target_temperature(23);
  call.perform();
  EXPECT_EQ(f.climate.get_custom_preset(), "Sleep 9h");
  set_preset(f.climate, "NONE");
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Low+");
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x63, 0x00, 0x02}, GENERIC), 9);
}

TEST(ToshibaWhTa15pe, PresetReplacementAndHadaMotionRestrictions) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  set_preset(f.climate, "Fireplace 2");
  set_preset(f.climate, "ECO");
  EXPECT_FALSE(f.climate.has_custom_preset());
  set_preset(f.climate, "HADA Care");
  EXPECT_EQ(f.climate.get_custom_preset(), "HADA Care");
  const auto transmissions = f.transmitter.frames.size();
  f.climate.fix();
  auto call = f.climate.make_call();
  call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  call.perform();
  EXPECT_EQ(f.transmitter.frames.size(), transmissions);
  set_preset(f.climate, "NONE");
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_FALSE(f.climate.has_custom_preset());
  f.climate.fix();
  EXPECT_EQ(f.transmitter.frames.size(), transmissions + 2);
}

TEST(ToshibaWhTa15pe, FanOnlyDoesNotOverwriteRememberedTarget) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 23, climate::CLIMATE_FAN_LOW);
  ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x64, 0x00, 0x35}, GENERIC));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 23);
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Low+");
}

TEST(ToshibaWhTa15pe, RejectsEveryTruncatedCaptureAndBadChecksumWithoutPartialState) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  set_fan(f.climate, "Medium+");
  set_preset(f.climate, "ECO");
  for (const auto &state : CAPTURED_STATES) {
    SCOPED_TRACE(frame_bytes(waveform(state.bytes, GENERIC), state.bytes.size()));
    auto raw = waveform(state.bytes, GENERIC, 1);
    raw.pop_back();  // Captured frames may end on the footer mark, without an idle space.
    for (size_t count = 0; count < raw.size(); count++) {
      remote_base::RawTimings truncated(raw.begin(), raw.begin() + count);
      remote_base::RemoteReceiveData data(truncated, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
      EXPECT_FALSE(static_cast<remote_base::RemoteReceiverListener &>(f.climate).on_receive(data));
    }
    auto corrupt = state.bytes;
    corrupt.back() ^= 1;
    EXPECT_FALSE(receive(f.climate, corrupt, GENERIC));
    EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
    EXPECT_EQ(f.climate.get_custom_fan_mode(), "Medium+");
    EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  }
}

TEST(ToshibaWhTa15pe, RejectsUnknownFormatsAndDoesNotChangeOtherModels) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  const std::vector<std::vector<uint8_t>> invalid = {
      {0xF2, 0x0D, 0x01, 0xFE, 0x01, 0x04, 0x05},
      {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x08, 0x29},
      {0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x00, 0xFF, 0xE5},
      {0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x70, 0x63, 0x00, 0x21, 0x08, 0x3B},
      {0xF2, 0x0D, 0x06, 0xF9, 0x0B, 0x70, 0x03, 0x00, 0x08, 0x7F, 0x03, 0x0C},
  };
  for (const auto &bytes : invalid)
    EXPECT_FALSE(receive(f.climate, bytes, GENERIC));
  Fixture generic(MODEL_GENERIC);
  EXPECT_FALSE(receive(generic.climate, {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC));
  EXPECT_FALSE(generic.climate.get_traits().get_supports_presets());
  EXPECT_TRUE(generic.climate.get_traits().get_supported_custom_fan_modes().empty());
}

TEST(ToshibaWhTa15pe, RebootDoesNotRearmSleepAndRestoresCustomFan) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 23, climate::CLIMATE_FAN_LOW);
  set_fan(f.climate, "Medium+");
  {
    Fixture restored(MODEL_RAS_25J2KVSG_ND);
    EXPECT_EQ(restored.climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_EQ(restored.climate.get_custom_fan_mode(), "Medium+");
    EXPECT_FALSE(restored.climate.fan_mode.has_value());
    EXPECT_FLOAT_EQ(restored.climate.target_temperature, 23);
    EXPECT_TRUE(restored.transmitter.frames.empty());
  }
  set_preset(f.climate, "Sleep 3h");
  Fixture restored(MODEL_RAS_25J2KVSG_ND);
  EXPECT_EQ(restored.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(restored.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_FALSE(restored.climate.has_custom_preset());
  EXPECT_TRUE(restored.transmitter.frames.empty());
  auto call = restored.climate.make_call();
  call.set_target_temperature(24);
  call.perform();
  expect_frame(restored.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x03, 0x00, 0x72}, GENERIC), 9);
}

TEST(ToshibaWhTa15pe, RestoresSelectedPowerBeforePublishingAndSendingTarget) {
  for (size_t selection = 0; selection <= 4; selection++) {
    for (const bool eco : {false, true}) {
      if (eco && selection >= 3)
        continue;
      SCOPED_TRACE(::testing::Message() << "selection " << selection << " eco " << eco);
      global_preferences->reset();
      Fixture f(MODEL_RAS_25J2KVSG_ND);
      f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
      f.climate.set_power_selection(selection);
      if (eco)
        set_preset(f.climate, "ECO");

      PowerSelection power;
      Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
      restored.climate.set_power_selection_select(&power);
      size_t publications = 0;
      power.add_on_state_callback([&](size_t index) {
        publications++;
        EXPECT_EQ(index, selection);
      });
      restored.climate.setup();
      EXPECT_EQ(publications, 1u);
      EXPECT_EQ(power.active_index(), selection);
      EXPECT_EQ(restored.climate.preset, eco ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE);
      EXPECT_TRUE(restored.transmitter.frames.empty());

      auto target = restored.climate.make_call();
      target.set_target_temperature(23);
      target.perform();
      ASSERT_EQ(restored.transmitter.frames.size(), 1u);
      // Derived regression frames: a new target must keep the saved power bits or Silent extension.
      const uint8_t bits = selection <= 2 ? selection << 6 : 0;
      std::vector<uint8_t> bytes{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x43, bits};
      if (eco || selection >= 3) {
        bytes[2] = 0x04;
        bytes[3] = 0xFB;
        bytes[4] = 0x09;
        bytes.push_back(eco ? 0x03 : (selection == 3 ? 0x02 : 0x0A));
      }
      bytes.push_back(xor_bytes(bytes.data() + 4, bytes.size() - 4));
      expect_frame(restored.last_frame(), waveform(bytes, GENERIC), bytes.size());
    }
  }
}

TEST(ToshibaWhTa15pe, RestoresReceivedPowerWithoutASelectConfiguredAtReception) {
  for (const auto &state : CAPTURED_STATES) {
    if (state.power == 0)
      continue;
    SCOPED_TRACE(state.power);
    global_preferences->reset();
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    ASSERT_TRUE(receive(f.climate, state.bytes, GENERIC));
    PowerSelection power;
    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    restored.climate.set_power_selection_select(&power);
    restored.climate.setup();
    EXPECT_EQ(power.active_index(), state.power);
    EXPECT_TRUE(restored.transmitter.frames.empty());
    auto target = restored.climate.make_call();
    target.set_target_temperature(state.temperature);
    target.perform();
    ASSERT_EQ(restored.transmitter.frames.size(), 1u);
    expect_frame(restored.last_frame(), waveform(state.bytes, GENERIC), state.bytes.size());
  }
}

TEST(ToshibaWhTa15pe, PublishesDefaultPowerAndIgnoresInvalidSavedSelections) {
  for (const int saved : {-1, 5, 255}) {
    global_preferences->reset();
    PowerSelection power;
    Fixture f(MODEL_RAS_25J2KVSG_ND, false);
    if (saved >= 0) {
      const uint8_t value = saved;
      auto pref = f.climate.make_entity_preference<uint8_t>(0x5371E24D);
      ASSERT_TRUE(pref.save(&value));
    }
    f.climate.set_power_selection_select(&power);
    f.climate.setup();
    EXPECT_EQ(power.active_index(), 0u);
    EXPECT_TRUE(f.transmitter.frames.empty());
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO);
    ASSERT_EQ(f.transmitter.frames.size(), 1u);
    expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x03, 0x00, 0x72}, GENERIC), 9);
  }
}

TEST(ToshibaWhTa15pe, KeepsSavedSilentAndClearsConflictingPresetsBeforePublishing) {
  for (const uint8_t silent : {3, 4}) {
    for (const auto &state : CAPTURED_STATES) {
      if (strcmp(state.preset, "NONE") == 0)
        continue;
      SCOPED_TRACE(::testing::Message() << "Silent " << silent << " preset " << state.preset);
      global_preferences->reset();
      Fixture f(MODEL_RAS_25J2KVSG_ND);
      ASSERT_TRUE(receive(f.climate, state.bytes, GENERIC));
      // Simulate inconsistent climate and power preferences after an interrupted save.
      auto pref = f.climate.make_entity_preference<uint8_t>(0x5371E24D);
      ASSERT_TRUE(pref.save(&silent));
      PowerSelection power;
      Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
      restored.climate.set_power_selection_select(&power);
      size_t publications = 0;
      restored.climate.add_on_state_callback([&](climate::Climate &climate) {
        publications++;
        EXPECT_EQ(climate.preset, climate::CLIMATE_PRESET_NONE);
        EXPECT_FALSE(climate.has_custom_preset());
        EXPECT_GE(climate.target_temperature, 17);
      });
      restored.climate.setup();
      EXPECT_GT(publications, 0u);
      EXPECT_EQ(power.active_index(), silent);
      EXPECT_TRUE(restored.transmitter.frames.empty());
    }
  }
}

TEST(ToshibaRestore, ExistingModelsRestoreTheirTraitsBeforePublishing) {
  for (const auto model : {MODEL_GENERIC, MODEL_RAC_PT1411HWRU_C, MODEL_RAC_PT1411HWRU_F, MODEL_RAS_2819T, MODEL_SEIYA,
                           MODEL_RAS_B10N3KV2}) {
    SCOPED_TRACE(model);
    global_preferences->reset();
    Fixture f(model);
    const auto traits = f.climate.get_traits();
    const auto swing = traits.get_supports_swing_modes() ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
    const float temperature = traits.get_visual_min_temperature();
    f.command(climate::CLIMATE_MODE_COOL, temperature, climate::CLIMATE_FAN_AUTO, swing);
    Fixture restored(model, false);
    size_t publications = 0;
    restored.climate.add_on_state_callback([&](climate::Climate &climate) {
      publications++;
      EXPECT_EQ(climate.swing_mode, swing);
      EXPECT_FLOAT_EQ(climate.target_temperature, temperature);
      EXPECT_FLOAT_EQ(climate.get_traits().get_visual_min_temperature(), temperature);
    });
    restored.climate.setup();
    EXPECT_GT(publications, 0u);
    EXPECT_TRUE(restored.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, RestoresSwingBeforePublishingAndDecidingWhetherToToggle) {
  for (const auto swing : {climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_OFF}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    set_preset(f.climate, "NONE");
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, swing);

    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    size_t publications = 0;
    bool restoring = true;
    restored.climate.add_on_state_callback([&](climate::Climate &state) {
      publications++;
      if (restoring)
        EXPECT_EQ(state.swing_mode, swing);
    });
    restored.climate.setup();
    EXPECT_GT(publications, 0u);
    EXPECT_EQ(restored.climate.swing_mode, swing);
    EXPECT_TRUE(restored.transmitter.frames.empty());
    restoring = false;

    auto off = restored.climate.make_call();
    off.set_swing_mode(climate::CLIMATE_SWING_OFF);
    off.perform();
    ASSERT_EQ(restored.transmitter.frames.size(), swing == climate::CLIMATE_SWING_VERTICAL ? 1u : 0u);
    if (swing == climate::CLIMATE_SWING_VERTICAL)
      expect_frame(restored.last_frame(), waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC), 7);
  }
}

TEST(ToshibaWhTa15pe, RestoreCallbacksChangingTargetDoNotToggleSwing) {
  for (const auto saved : {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                           climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {
    SCOPED_TRACE(saved);
    global_preferences->reset();
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO);
    // Include saved directions that are no longer supported by this model.
    f.climate.swing_mode = saved;
    f.climate.publish_state();

    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    const auto swing = saved == climate::CLIMATE_SWING_VERTICAL ? saved : climate::CLIMATE_SWING_OFF;
    size_t publications = 0;
    bool changed = false;
    bool restoring = true;
    restored.climate.add_on_state_callback([&](climate::Climate &state) {
      publications++;
      if (restoring)
        EXPECT_EQ(state.swing_mode, swing);
      if (!changed) {
        changed = true;
        auto target = restored.climate.make_call();
        target.set_target_temperature(23);
        target.perform();
      }
    });
    restored.climate.setup();
    restoring = false;
    EXPECT_EQ(publications, 2u);
    EXPECT_FLOAT_EQ(restored.climate.target_temperature, 23);
    ASSERT_EQ(restored.transmitter.frames.size(), 1u);
    expect_frame(restored.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x03, 0x00, 0x62}, GENERIC), 9);

    auto target = restored.climate.make_call();
    target.set_target_temperature(22);
    target.perform();
    ASSERT_EQ(restored.transmitter.frames.size(), 2u);
    expect_frame(restored.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x03, 0x00, 0x52}, GENERIC), 9);
    auto off = restored.climate.make_call();
    off.set_swing_mode(climate::CLIMATE_SWING_OFF);
    off.perform();
    EXPECT_EQ(restored.transmitter.frames.size(), swing == climate::CLIMATE_SWING_VERTICAL ? 3u : 2u);
    EXPECT_EQ(restored.climate.swing_mode, climate::CLIMATE_SWING_OFF);
  }
}

TEST(ToshibaWhTa15pe, RestoreCallbacksCanDeliberatelyChangeSwing) {
  for (const auto saved : {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {
    SCOPED_TRACE(saved);
    global_preferences->reset();
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO, saved);
    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    const auto swing =
        saved == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
    bool changed = false;
    restored.climate.add_on_state_callback([&](climate::Climate &state) {
      if (!changed) {
        changed = true;
        EXPECT_EQ(state.swing_mode, saved);
        auto call = restored.climate.make_call();
        call.set_target_temperature(23);
        call.set_swing_mode(swing);
        call.perform();
      }
    });
    restored.climate.setup();
    ASSERT_TRUE(changed);
    EXPECT_EQ(restored.climate.swing_mode, swing);
    auto expected = waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x03, 0x00, 0x62}, GENERIC);
    const auto motion = waveform({0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25}, GENERIC);
    expected.insert(expected.end(), motion.begin(), motion.end());
    ASSERT_EQ(restored.transmitter.frames.size(), 1u);
    EXPECT_EQ(restored.last_frame(), expected);

    auto target = restored.climate.make_call();
    target.set_target_temperature(22);
    target.perform();
    ASSERT_EQ(restored.transmitter.frames.size(), 2u);
    expect_frame(restored.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x03, 0x00, 0x52}, GENERIC), 9);
  }
}

TEST(ToshibaWhTa15pe, ClearsRestoredSleepBeforeAnyPublication) {
  for (const char *preset : {"Sleep 1h", "Sleep 3h", "Sleep 5h", "Sleep 9h"}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    f.command(climate::CLIMATE_MODE_HEAT, 23, climate::CLIMATE_FAN_AUTO);
    set_preset(f.climate, preset);

    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    size_t publications = 0;
    restored.climate.add_on_state_callback([&](climate::Climate &state) {
      publications++;
      EXPECT_FALSE(state.has_custom_preset());
      EXPECT_EQ(state.preset, climate::CLIMATE_PRESET_NONE);
      EXPECT_FLOAT_EQ(state.target_temperature, 23);
    });
    restored.climate.setup();
    EXPECT_GT(publications, 0u);
    EXPECT_TRUE(restored.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, SanitizesInvalidRestoredHeatingPresetsBeforePublishing) {
  for (const char *preset : {"Fireplace 1", "Fireplace 2", "Frost protection"}) {
    for (const bool heat_supported : {true, false}) {
      Fixture f(MODEL_RAS_25J2KVSG_ND);
      f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
      set_preset(f.climate, preset);
      // Save a state from a configuration that is no longer valid at startup.
      if (heat_supported)
        f.climate.mode = climate::CLIMATE_MODE_COOL;
      f.climate.target_temperature = NAN;
      f.climate.publish_state();

      Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
      restored.climate.set_supports_heat(heat_supported);
      size_t publications = 0;
      restored.climate.add_on_state_callback([&](climate::Climate &state) {
        publications++;
        EXPECT_FALSE(state.has_custom_preset());
        EXPECT_EQ(state.preset, climate::CLIMATE_PRESET_NONE);
        EXPECT_EQ(state.mode, heat_supported ? climate::CLIMATE_MODE_COOL : climate::CLIMATE_MODE_OFF);
        EXPECT_FLOAT_EQ(state.target_temperature, 24);
      });
      restored.climate.setup();
      EXPECT_GT(publications, 0u);
      EXPECT_TRUE(restored.transmitter.frames.empty());
    }
  }
}

TEST(ToshibaWhTa15pe, RestoresValidPresetsAndTargetsBeforePublishing) {
  for (const auto &state : CAPTURED_STATES) {
    if (strcmp(state.preset, "NONE") == 0 || strncmp(state.preset, "Sleep", 5) == 0)
      continue;
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    ASSERT_TRUE(receive(f.climate, state.bytes, GENERIC));
    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    size_t publications = 0;
    restored.climate.add_on_state_callback([&](climate::Climate &climate) {
      publications++;
      EXPECT_EQ(climate.preset, f.climate.preset);
      EXPECT_EQ(climate.get_custom_preset(), f.climate.get_custom_preset());
      EXPECT_EQ(climate.mode, f.climate.mode);
      EXPECT_FLOAT_EQ(climate.target_temperature, state.temperature);
    });
    restored.climate.setup();
    EXPECT_GT(publications, 0u);
    EXPECT_TRUE(restored.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, ClearsUnsupportedRestoredPresetAndClampsNormalTargetBeforePublishing) {
  for (const float temperature : {5.0f, 31.0f, NAN}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    set_preset(f.climate, "NONE");
    f.climate.preset = climate::CLIMATE_PRESET_HOME;
    f.climate.target_temperature = temperature;
    f.climate.publish_state();

    Fixture restored(MODEL_RAS_25J2KVSG_ND, false);
    size_t publications = 0;
    restored.climate.add_on_state_callback([&](climate::Climate &state) {
      publications++;
      EXPECT_EQ(state.preset, climate::CLIMATE_PRESET_NONE);
      EXPECT_FALSE(state.has_custom_preset());
      EXPECT_FLOAT_EQ(state.target_temperature, std::isnan(temperature) ? 24 : (temperature < 17 ? 17 : 30));
    });
    restored.climate.setup();
    EXPECT_GT(publications, 0u);
    EXPECT_TRUE(restored.transmitter.frames.empty());
  }
}

TEST(ToshibaWhTa15pe, HadaAllowsOffSwingButRejectsIncompatibleMovementAtomically) {
  for (const auto swing : {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {
    Fixture f(MODEL_RAS_25J2KVSG_ND);
    set_preset(f.climate, "NONE");
    f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW, swing);
    const auto transmissions = f.transmitter.frames.size();
    auto hada = f.climate.make_call();
    hada.set_preset("HADA Care");
    hada.set_swing_mode(climate::CLIMATE_SWING_OFF);
    hada.perform();
    EXPECT_EQ(f.climate.get_custom_preset(), "HADA Care");
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
    ASSERT_EQ(f.transmitter.frames.size(), transmissions + 1);
    expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x05, 0xFA, 0x01, 0x70, 0x43, 0x00, 0x21, 0x17, 0x04}, GENERIC),
                 11);

    auto update = f.climate.make_call();
    update.set_target_temperature(23);
    update.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    update.perform();
    EXPECT_EQ(f.transmitter.frames.size(), transmissions + 1);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
    EXPECT_EQ(f.climate.get_custom_preset(), "HADA Care");
    EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_OFF);
  }
}

TEST(ToshibaWhTa15pe, PreservesExplicitEcoAndBoostWhileOffButCancelsOnPowerOff) {
  for (const auto preset : {climate::CLIMATE_PRESET_ECO, climate::CLIMATE_PRESET_BOOST}) {
    for (const bool explicit_off : {true, false}) {
      Fixture f(MODEL_RAS_25J2KVSG_ND);
      f.command(climate::CLIMATE_MODE_OFF, 24, climate::CLIMATE_FAN_AUTO);
      f.climate.set_power_selection(1);
      const auto transmissions = f.transmitter.frames.size();
      auto call = f.climate.make_call();
      call.set_preset(preset);
      if (explicit_off)
        call.set_mode(climate::CLIMATE_MODE_OFF);
      call.perform();
      EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
      EXPECT_EQ(f.climate.preset, preset);
      ASSERT_EQ(f.transmitter.frames.size(), transmissions + 1);

      Fixture rx(MODEL_RAS_25J2KVSG_ND);
      remote_base::RemoteReceiveData data(f.last_frame(), 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
      ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(rx.climate).on_receive(data));
      EXPECT_EQ(rx.climate.preset, preset);
      EXPECT_EQ(rx.climate.mode, climate::CLIMATE_MODE_OFF);

      auto temperature = f.climate.make_call();
      temperature.set_target_temperature(23);
      temperature.perform();
      EXPECT_EQ(f.climate.preset, preset);
      auto off = f.climate.make_call();
      off.set_mode(climate::CLIMATE_MODE_OFF);
      off.perform();
      EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
      expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x60, 0x07, 0x40, 0x26}, GENERIC), 9);
    }
  }
}

TEST(ToshibaWhTa15pe, RejectsOnlyUnverifiedSilentPresetCombinationsWithoutChangingState) {
  for (const auto &state : CAPTURED_STATES) {
    if (strcmp(state.preset, "NONE") == 0)
      continue;
    for (const size_t silent : {3u, 4u}) {
      for (const bool silent_first : {true, false}) {
        SCOPED_TRACE(::testing::Message() << state.preset << " Silent " << silent << " first " << silent_first);
        global_preferences->reset();
        PowerSelection power;
        Fixture f(MODEL_RAS_25J2KVSG_ND);
        f.climate.set_power_selection_select(&power);
        set_preset(f.climate, "NONE");
        f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
        f.climate.set_power_selection(0);
        if (silent_first) {
          f.climate.set_power_selection(silent);
        } else {
          set_preset(f.climate, state.preset);
        }
        const auto transmissions = f.transmitter.frames.size();
        const auto preset = f.climate.preset;
        const auto custom_preset = f.climate.get_custom_preset();
        const auto temperature = f.climate.target_temperature;
        if (silent_first) {
          auto call = f.climate.make_call();
          call.set_preset(state.preset);
          call.set_target_temperature(23);
          call.set_fan_mode(climate::CLIMATE_FAN_HIGH);
          call.perform();
          EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_LOW);
        } else {
          f.climate.set_power_selection(silent);
        }
        EXPECT_EQ(f.transmitter.frames.size(), transmissions);
        EXPECT_EQ(f.climate.preset, preset);
        EXPECT_EQ(f.climate.get_custom_preset(), custom_preset);
        EXPECT_FLOAT_EQ(f.climate.target_temperature, temperature);
        EXPECT_EQ(power.active_index(), silent_first ? silent : 0u);

        PowerSelection rx_power;
        Fixture rx(MODEL_RAS_25J2KVSG_ND);
        rx.climate.set_power_selection_select(&rx_power);
        remote_base::RemoteReceiveData data(f.last_frame(), 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
        ASSERT_TRUE(static_cast<remote_base::RemoteReceiverListener &>(rx.climate).on_receive(data));
        EXPECT_EQ(rx_power.active_index(), power.active_index());
        EXPECT_EQ(rx.climate.preset, preset);
        EXPECT_EQ(rx.climate.get_custom_preset(), custom_preset);

        // Either cancel the preset or leave Silent before selecting the other feature.
        if (silent_first) {
          f.climate.set_power_selection(1);
          set_preset(f.climate, state.preset);
          EXPECT_EQ(power.active_index(), 1u);
        } else {
          set_preset(f.climate, "NONE");
          f.climate.set_power_selection(silent);
          EXPECT_EQ(power.active_index(), silent);
        }
        EXPECT_EQ(f.transmitter.frames.size(), transmissions + 2);
      }
    }
  }
}

TEST(ToshibaWhTa15pe, ModeChangeExitsHeatingPresetAndPreservesNormalTarget) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  set_preset(f.climate, "Frost protection");
  auto call = f.climate.make_call();
  call.set_mode(climate::CLIMATE_MODE_COOL);
  call.perform();
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);

  const auto transmissions = f.transmitter.frames.size();
  call.set_preset("Fireplace 1");
  call.perform();
  EXPECT_EQ(f.transmitter.frames.size(), transmissions);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);

  f.climate.set_supports_heat(false);
  set_preset(f.climate, "Frost protection");
  EXPECT_EQ(f.transmitter.frames.size(), transmissions);
}

TEST(ToshibaWhTa15pe, PresetsPreservePowerLimitsInBothControlOrders) {
  // The power limit occupies byte 7 independently of the preset's feature bytes.
  // Combined payloads are regression expectations, not additional handheld captures.
  for (const auto &state : CAPTURED_STATES) {
    if (strcmp(state.preset, "NONE") == 0)
      continue;
    for (const size_t power : {1u, 2u}) {
      SCOPED_TRACE(::testing::Message() << "preset " << state.preset << " power " << power);
      auto expected = state.bytes;
      expected[7] |= power << 6;
      expected.back() ^= power << 6;

      for (const bool power_first : {true, false}) {
        Fixture f(MODEL_RAS_25J2KVSG_ND);
        auto initial = f.climate.make_call();
        initial.set_mode(climate::CLIMATE_MODE_HEAT);
        initial.set_target_temperature(24);
        initial.set_fan_mode(climate::CLIMATE_FAN_AUTO);
        initial.set_preset(climate::CLIMATE_PRESET_NONE);
        initial.perform();
        if (power_first)
          f.climate.set_power_selection(power);
        const auto transmissions = f.transmitter.frames.size();
        auto call = f.climate.make_call();
        call.set_mode(state.mode);
        call.set_target_temperature(state.temperature);
        call.set_fan_mode(state.fan);
        call.set_preset(state.preset);
        call.perform();
        ASSERT_EQ(f.transmitter.frames.size(), transmissions + 1);
        if (!power_first)
          f.climate.set_power_selection(power);
        expect_frame(f.last_frame(), waveform(expected, GENERIC), expected.size());

        // A subsequent ordinary target update must retain both settings.
        const auto updates = f.transmitter.frames.size();
        auto temperature = f.climate.make_call();
        temperature.set_target_temperature(state.temperature);
        temperature.perform();
        ASSERT_EQ(f.transmitter.frames.size(), updates + 1);
        expect_frame(f.last_frame(), waveform(expected, GENERIC), expected.size());
      }
    }
  }
}

TEST(ToshibaWhTa15pe, ReceivesCombinedPresetsAndPowerLimitsWithoutLosingEither) {
  PowerSelection power_select;
  for (const auto &state : CAPTURED_STATES) {
    if (strcmp(state.preset, "NONE") == 0)
      continue;
    for (const size_t power : {1u, 2u}) {
      SCOPED_TRACE(::testing::Message() << "preset " << state.preset << " power " << power);
      auto combined = state.bytes;
      combined[7] |= power << 6;
      combined.back() ^= power << 6;
      Fixture f(MODEL_RAS_25J2KVSG_ND);
      f.climate.set_power_selection_select(&power_select);
      ASSERT_TRUE(receive(f.climate, combined, GENERIC));
      EXPECT_EQ(power_select.active_index(), power);
      EXPECT_FLOAT_EQ(f.climate.target_temperature, state.temperature);
      const auto preset = f.climate.preset;
      const auto custom_preset = f.climate.get_custom_preset();

      // Setting the same preset after receiving it must transmit and keep the limit.
      const auto transmissions = f.transmitter.frames.size();
      set_preset(f.climate, state.preset);
      ASSERT_EQ(f.transmitter.frames.size(), transmissions + 1);
      EXPECT_EQ(f.climate.preset, preset);
      EXPECT_EQ(f.climate.get_custom_preset(), custom_preset);
      expect_frame(f.last_frame(), waveform(combined, GENERIC), combined.size());
    }
  }
}

TEST(ToshibaWhTa15pe, CancellingPresetPreservesPowerLimitAndPublishesSelection) {
  PowerSelection power_select;
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.climate.set_power_selection_select(&power_select);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_AUTO);
  set_fan(f.climate, "Low+");
  f.climate.set_power_selection(1);
  set_preset(f.climate, "ECO");
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_EQ(power_select.active_index(), 1u);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x40, 0x03, 0x59}, GENERIC), 10);
  f.climate.set_power_selection(2);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_EQ(power_select.active_index(), 2u);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x04, 0xFB, 0x09, 0x70, 0x63, 0x80, 0x03, 0x99}, GENERIC), 10);
  set_preset(f.climate, "NONE");
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_EQ(power_select.active_index(), 2u);
  expect_frame(f.last_frame(), waveform({0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x80, 0x92}, GENERIC), 9);
}

TEST(ToshibaWhTa15pe, SensorUpdatesDoNotTransmitARacComfortSenseCommand) {
  sensor::Sensor sensor;
  CapturingTransmitter transmitter;
  ToshibaClimate climate;
  climate.set_model(MODEL_RAS_25J2KVSG_ND);
  climate.set_transmitter(&transmitter);
  climate.set_sensor(&sensor);
  climate.setup();
  sensor.publish_state(21);
  EXPECT_FLOAT_EQ(climate.current_temperature, 21);
  EXPECT_TRUE(transmitter.frames.empty());
}

TEST(ToshibaWhTa15pe, RejectsInvalidStateFieldsEvenWithValidChecksums) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 23, climate::CLIMATE_FAN_LOW);
  for (const auto &field : std::vector<std::pair<size_t, uint8_t>>{
           {4, 0x05}, {5, 0x61}, {5, 0xE0}, {6, 0x65}, {6, 0xE3}, {6, 0x73}, {7, 0x10}, {7, 0xC0}, {7, 0x01}}) {
    auto bytes = std::vector<uint8_t>{0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x00, 0x12};
    bytes[field.first] = field.second;
    bytes.back() = xor_bytes(bytes.data() + 4, bytes.size() - 5);
    EXPECT_FALSE(receive(f.climate, bytes, GENERIC));
    EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 23);
    EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_LOW);
  }
  EXPECT_FALSE(receive(f.climate, {0x00, 0xFF, 0x03, 0xFC, 0x01, 0x70, 0x63, 0x00, 0x12}, GENERIC));
}

TEST(ToshibaWhTa15pe, FanOnlyAfterFrostRestoresNormalTargetWithoutDecodingItsTemperatureByte) {
  Fixture f(MODEL_RAS_25J2KVSG_ND);
  f.command(climate::CLIMATE_MODE_HEAT, 24, climate::CLIMATE_FAN_LOW);
  ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x04, 0xFB, 0x09, 0xC0, 0x63, 0x00, 0x04, 0xAE}, GENERIC));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 13);
  ASSERT_TRUE(receive(f.climate, {0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x50, 0x64, 0x00, 0x35}, GENERIC));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_FAN_ONLY);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24);
}

}  // namespace esphome::toshiba::testing
