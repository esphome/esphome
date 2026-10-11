#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/toshiba/toshiba.h"

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
static remote_base::RawTimings waveform(const std::vector<uint8_t> &bytes, const Timings &t) {
  remote_base::RawTimings raw;
  for (int copy = 0; copy < 2; copy++) {
    raw.push_back(t.header_mark);
    raw.push_back(-static_cast<int32_t>(t.header_space));
    for (uint8_t byte : bytes) {
      for (int bit = 7; bit >= 0; bit--) {
        raw.push_back(t.bit_mark);
        raw.push_back(-static_cast<int32_t>((byte >> bit) & 1 ? t.one_space : t.zero_space));
      }
    }
    raw.push_back(t.bit_mark);
    if (copy == 0 || t.gap_after_last) {
      raw.push_back(-static_cast<int32_t>(t.gap_space));
    }
  }
  return raw;
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
  ToshibaClimate climate;

  explicit Fixture(Model model) {
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

  const remote_base::RawTimings &last_frame() const { return this->transmitter.frames.back(); }
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

}  // namespace esphome::toshiba::testing
