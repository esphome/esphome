#include <gtest/gtest.h>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

#include "esphome/components/mistral_ir/mistral_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome::mistral_ir::testing {

using Frame = std::vector<uint8_t>;

static constexpr uint8_t POWER_OFF = 0x00;
static constexpr uint8_t POWER_ON = 0x20;
static constexpr uint8_t MODE_COOL = 0xC0;
static constexpr uint8_t MODE_DRY = 0x40;
static constexpr uint8_t MODE_FAN_ONLY = 0x80;
static constexpr uint8_t MODE_HEAT = 0x60;
static constexpr uint8_t MODE_AUTO = 0x10;
static constexpr uint8_t FAN_AUTO_BYTE1 = 0xA4;
static constexpr uint8_t FAN_AUTO_BYTE6 = 0x00;
static constexpr uint8_t FAN_LOW_BYTE1 = 0x44;
static constexpr uint8_t FAN_LOW_BYTE6 = 0x40;
static constexpr uint8_t FAN_MEDIUM_BYTE1 = 0x24;
static constexpr uint8_t FAN_MEDIUM_BYTE6 = 0xC0;
static constexpr uint8_t FAN_HIGH_BYTE1 = 0xA4;
static constexpr uint8_t FAN_HIGH_BYTE6 = 0xA0;
static constexpr uint16_t WRONG_ADDRESS = 0x322D;
static constexpr uint32_t TOLERANCE_PERCENT = 25;

static constexpr climate::ClimateMode ALL_MODES[] = {
    climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY,       climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_HEAT_COOL,
};
static constexpr climate::ClimateFanMode ALL_FANS[] = {
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
};

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
  }
};

static uint8_t checksum(const Frame &frame) {
  uint8_t sum = 0;
  for (size_t idx : {1, 3, 4, 5, 6, 8})
    sum += reverse_bits(frame[idx]);
  return reverse_bits(sum);
}

/// A received frame with a valid checksum, which callers may overwrite
static Frame make_frame(uint8_t byte1, uint8_t power, uint8_t mode, uint8_t temperature, uint8_t byte6) {
  Frame frame{0x56,  byte1, 0x08, power, mode, reverse_bits(static_cast<uint8_t>(0x9F - temperature)),
              byte6, 0x00,  0xE8, 0x00,  0x60, 0x00};
  frame[11] = checksum(frame);
  return frame;
}

static remote_base::RawTimings encode(uint16_t address, const Frame &frame) {
  remote_base::RemoteTransmitData data;
  remote_base::AEHAProtocol().encode(&data, address, frame.data(), frame.size());
  return data.get_data();
}

struct Fixture {
  CapturingTransmitter transmitter;
  MistralIR climate;

  Fixture() {
    // Every fixture shares one preference key, so clear what an earlier test saved or setup restores it
    if (global_preferences != nullptr)
      global_preferences->reset();
    // The test main does not construct App as generated code does
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    static char name[] = "test_mistral_ir";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->climate.set_transmitter(&this->transmitter);
    static_cast<Component &>(this->climate).setup();
  }

  void command(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_fan_mode(fan);
    call.perform();
  }

  /// The 12 data bytes of the last transmission, decoded by the AEHA protocol
  Frame last_frame() const {
    EXPECT_FALSE(this->transmitter.frames.empty());
    if (this->transmitter.frames.empty())
      return {};
    remote_base::RemoteReceiveData data(this->transmitter.frames.back(), TOLERANCE_PERCENT,
                                        remote_base::TOLERANCE_MODE_PERCENTAGE);
    const auto aeha = remote_base::AEHAProtocol().decode(data);
    EXPECT_TRUE(aeha.has_value());
    if (!aeha.has_value())
      return {};
    EXPECT_EQ(aeha->address, MISTRAL_ADDRESS);
    EXPECT_EQ(aeha->data.size(), MISTRAL_FRAME_SIZE);
    return aeha->data;
  }

  bool receive(const remote_base::RawTimings &raw) {
    remote_base::RemoteReceiveData data(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
    // The receiver calls through the listener interface, where on_receive is public
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(data);
  }

  bool receive(const Frame &frame, uint16_t address = MISTRAL_ADDRESS) { return this->receive(encode(address, frame)); }

  void expect_state(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan) const {
    EXPECT_EQ(this->climate.mode, mode);
    EXPECT_FLOAT_EQ(this->climate.target_temperature, temperature);
    EXPECT_EQ(this->climate.fan_mode, fan);
  }
};

TEST(MistralIR, EncodesEachMode) {
  struct Case {
    climate::ClimateMode mode;
    uint8_t code;
    uint8_t checksum;
  };
  // 22 C: 0x9F - 22 = 0x89, reversed 0x91. Checksum sums the reversed bytes 1, 3, 4, 5, 6 and 8:
  // fan AUTO 0xA4 -> 0x25, power on 0x20 -> 0x04, mode, temperature 0x89, byte 6 0x00, 0xE8 -> 0x17
  const Case cases[] = {
      // COOL 0xC0 -> 0x03: 0x25 + 0x04 + 0x03 + 0x89 + 0x00 + 0x17 = 0xCC, reversed 0x33
      {climate::CLIMATE_MODE_COOL, MODE_COOL, 0x33},
      // DRY 0x40 -> 0x02: sum 0xCB, reversed 0xD3
      {climate::CLIMATE_MODE_DRY, MODE_DRY, 0xD3},
      // FAN_ONLY 0x80 -> 0x01: sum 0xCA, reversed 0x53
      {climate::CLIMATE_MODE_FAN_ONLY, MODE_FAN_ONLY, 0x53},
      // HEAT 0x60 -> 0x06: sum 0xCF, reversed 0xF3
      {climate::CLIMATE_MODE_HEAT, MODE_HEAT, 0xF3},
      // HEAT_COOL 0x10 -> 0x08: sum 0xD1, reversed 0x8B
      {climate::CLIMATE_MODE_HEAT_COOL, MODE_AUTO, 0x8B},
  };
  Fixture f;
  for (const Case &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.mode));
    f.command(c.mode, 22.0f, climate::CLIMATE_FAN_AUTO);
    const Frame expected{0x56,           FAN_AUTO_BYTE1, 0x08, POWER_ON, c.code, 0x91,
                         FAN_AUTO_BYTE6, 0x00,           0xE8, 0x00,     0x60,   c.checksum};
    EXPECT_EQ(f.last_frame(), expected);
  }
}

TEST(MistralIR, EncodesEachFanMode) {
  struct Case {
    climate::ClimateFanMode fan;
    uint8_t byte1;
    uint8_t byte6;
    uint8_t checksum;
  };
  // COOL at 24 C: 0x9F - 24 = 0x87, reversed 0xE1. Fixed part of the checksum sum:
  // power on 0x04 + COOL 0x03 + temperature 0x87 + 0xE8 -> 0x17 = 0xA5, plus reversed bytes 1 and 6
  const Case cases[] = {
      // AUTO 0xA4 -> 0x25, 0x00 -> 0x00: 0xA5 + 0x25 + 0x00 = 0xCA, reversed 0x53
      {climate::CLIMATE_FAN_AUTO, FAN_AUTO_BYTE1, FAN_AUTO_BYTE6, 0x53},
      // LOW 0x44 -> 0x22, 0x40 -> 0x02: 0xA5 + 0x22 + 0x02 = 0xC9, reversed 0x93
      {climate::CLIMATE_FAN_LOW, FAN_LOW_BYTE1, FAN_LOW_BYTE6, 0x93},
      // MEDIUM 0x24 -> 0x24, 0xC0 -> 0x03: 0xA5 + 0x24 + 0x03 = 0xCC, reversed 0x33
      {climate::CLIMATE_FAN_MEDIUM, FAN_MEDIUM_BYTE1, FAN_MEDIUM_BYTE6, 0x33},
      // HIGH 0xA4 -> 0x25, 0xA0 -> 0x05: 0xA5 + 0x25 + 0x05 = 0xCF, reversed 0xF3
      {climate::CLIMATE_FAN_HIGH, FAN_HIGH_BYTE1, FAN_HIGH_BYTE6, 0xF3},
  };
  Fixture f;
  for (const Case &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.fan));
    f.command(climate::CLIMATE_MODE_COOL, 24.0f, c.fan);
    const Frame frame = f.last_frame();
    ASSERT_EQ(frame.size(), MISTRAL_FRAME_SIZE);
    EXPECT_EQ(frame[1], c.byte1);
    EXPECT_EQ(frame[4], MODE_COOL);
    EXPECT_EQ(frame[5], 0xE1);
    EXPECT_EQ(frame[6], c.byte6);
    EXPECT_EQ(frame[11], c.checksum);
  }
}

TEST(MistralIR, EncodesOff) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_OFF, 22.0f, climate::CLIMATE_FAN_AUTO);
  // OFF has no mode code, so byte 4 falls back to auto 0x10 -> 0x08 with power 0x00:
  // 0x25 + 0x00 + 0x08 + 0x89 + 0x00 + 0x17 = 0xCD, reversed 0xB3
  const Frame expected{0x56,           FAN_AUTO_BYTE1, 0x08, POWER_OFF, MODE_AUTO, 0x91,
                       FAN_AUTO_BYTE6, 0x00,           0xE8, 0x00,      0x60,      0xB3};
  EXPECT_EQ(f.last_frame(), expected);
}

TEST(MistralIR, EncodesTemperatureRoundedAndClamped) {
  struct Case {
    float requested;
    uint8_t byte5;
  };
  const Case cases[] = {
      {16.0f, 0xF1},  // 0x9F - 16 = 0x8F, reversed 0xF1
      {22.0f, 0x91},  // 0x9F - 22 = 0x89, reversed 0x91
      {30.0f, 0x81},  // 0x9F - 30 = 0x81, reversed 0x81
      {15.6f, 0xF1},  // rounds to 16
      {30.4f, 0x81},  // rounds to 30
      {10.0f, 0xF1},  // clamps to 16
      {35.0f, 0x81},  // clamps to 30
  };
  Fixture f;
  for (const Case &c : cases) {
    SCOPED_TRACE(c.requested);
    f.command(climate::CLIMATE_MODE_COOL, c.requested, climate::CLIMATE_FAN_AUTO);
    const Frame frame = f.last_frame();
    ASSERT_EQ(frame.size(), MISTRAL_FRAME_SIZE);
    EXPECT_EQ(frame[5], c.byte5);
    EXPECT_EQ(frame[11], checksum(frame));
  }
}

TEST(MistralIR, RoundTripsEveryModeAndFan) {
  for (climate::ClimateMode mode : ALL_MODES) {
    for (climate::ClimateFanMode fan : ALL_FANS) {
      for (float temperature : {16.0f, 23.0f, 30.0f}) {
        SCOPED_TRACE(static_cast<int>(mode) * 1000 + static_cast<int>(fan) * 100 + static_cast<int>(temperature));
        Fixture sender;
        sender.command(mode, temperature, fan);
        EXPECT_EQ(sender.last_frame().size(), MISTRAL_FRAME_SIZE);
        Fixture receiver;
        EXPECT_TRUE(receiver.receive(sender.transmitter.frames.back()));
        receiver.expect_state(mode, temperature, fan);
      }
    }
  }
}

TEST(MistralIR, ReceivesHeatFrame) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  EXPECT_TRUE(f.receive(make_frame(FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, 25, FAN_LOW_BYTE6)));
  f.expect_state(climate::CLIMATE_MODE_HEAT, 25.0f, climate::CLIMATE_FAN_LOW);
}

TEST(MistralIR, ReceivedPowerOffKeepsTemperatureAndFan) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  EXPECT_TRUE(f.receive(make_frame(FAN_AUTO_BYTE1, POWER_OFF, MODE_AUTO, 28, FAN_AUTO_BYTE6)));
  f.expect_state(climate::CLIMATE_MODE_OFF, 22.0f, climate::CLIMATE_FAN_HIGH);
}

TEST(MistralIR, RejectsBadChecksum) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  Frame frame = make_frame(FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, 25, FAN_LOW_BYTE6);
  frame[11] ^= 0x01;
  EXPECT_FALSE(f.receive(frame));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
}

TEST(MistralIR, RejectsChangedFixedByte) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  for (size_t idx : {0, 2, 7, 8, 9, 10}) {
    SCOPED_TRACE(idx);
    Frame frame = make_frame(FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, 25, FAN_LOW_BYTE6);
    frame[idx] ^= 0x01;
    // Byte 8 is part of the checksum, so keep it valid to test the fixed-byte check alone
    frame[11] = checksum(frame);
    EXPECT_FALSE(f.receive(frame));
    f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  }
}

TEST(MistralIR, RejectsUnknownPowerModeAndFan) {
  struct Case {
    const char *name;
    uint8_t byte1;
    uint8_t power;
    uint8_t mode;
    uint8_t byte6;
  };
  const Case cases[] = {
      {"power 0x10", FAN_LOW_BYTE1, 0x10, MODE_HEAT, FAN_LOW_BYTE6},
      {"mode 0x20", FAN_LOW_BYTE1, POWER_ON, 0x20, FAN_LOW_BYTE6},
      {"fan 0x44/0x00", FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, FAN_AUTO_BYTE6},
      {"fan 0x00/0x40", 0x00, POWER_ON, MODE_HEAT, FAN_LOW_BYTE6},
  };
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  for (const Case &c : cases) {
    SCOPED_TRACE(c.name);
    EXPECT_FALSE(f.receive(make_frame(c.byte1, c.power, c.mode, 25, c.byte6)));
    f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  }
}

TEST(MistralIR, RejectsWrongAddress) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  const Frame frame = make_frame(FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, 25, FAN_LOW_BYTE6);
  EXPECT_FALSE(f.receive(frame, WRONG_ADDRESS));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
}

TEST(MistralIR, RejectsWrongFrameLength) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
  const Frame frame = make_frame(FAN_LOW_BYTE1, POWER_ON, MODE_HEAT, 25, FAN_LOW_BYTE6);
  Frame shorter(frame.begin(), frame.begin() + 11);
  EXPECT_FALSE(f.receive(shorter));
  Frame longer = frame;
  longer.push_back(0x00);
  EXPECT_FALSE(f.receive(longer));
  f.expect_state(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_FAN_HIGH);
}

}  // namespace esphome::mistral_ir::testing
