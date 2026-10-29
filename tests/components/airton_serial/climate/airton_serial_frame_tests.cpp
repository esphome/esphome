#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/airton_serial/airton_serial_climate.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "../common.h"

namespace esphome::airton_serial::testing {

namespace {

void set_crc(std::vector<uint8_t> &frame) {
  const uint16_t crc = crc16(frame.data(), frame.size() - 2);
  frame[frame.size() - 2] = crc >> 8;
  frame[frame.size() - 1] = crc & 0xFF;
}

// A3 state frame from the climate board, by default on, cool, fan high, 24 C setpoint, 22 C ambient.
std::vector<uint8_t> a3_frame(uint8_t swing = 0, uint8_t state = 0x59, uint8_t setpoint = 24 - 16) {
  std::vector<uint8_t> frame(34, 0);
  frame[0] = 0x7A;
  frame[1] = 0x7A;
  frame[2] = 0xD5;
  frame[3] = 0x21;
  frame[4] = 34;
  frame[7] = 0xA3;
  frame[10] = 22;
  frame[13] = state;
  frame[14] = setpoint;
  frame[15] = swing;
  frame[16] = 0x80;
  set_crc(frame);
  return frame;
}

class AirtonSerialFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.mode = climate::CLIMATE_MODE_OFF;
  }

  void expect_a3_applied_() {
    EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(this->sut_.target_temperature, 24.0f);
    EXPECT_FLOAT_EQ(this->sut_.current_temperature, 22.0f);
    ASSERT_TRUE(this->sut_.fan_mode.has_value());
    EXPECT_EQ(*this->sut_.fan_mode, climate::CLIMATE_FAN_HIGH);
  }

  void loop_at_(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};
    this->sut_.loop();
  }

  // Byte of the last A1 frame sent.
  uint8_t sent_(size_t index) const {
    EXPECT_GE(this->uart_.tx.size(), 24u);
    return this->uart_.tx[this->uart_.tx.size() - 24 + index];
  }

  void receive_state_(uint8_t swing = 0, uint8_t state = 0x59, uint8_t setpoint = 24 - 16) {
    this->uart_.push_rx(a3_frame(swing, state, setpoint));
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  AirtonSerialClimate sut_;
};

}  // namespace

TEST_F(AirtonSerialFrameTest, ValidFrameAfterNoise) {
  this->uart_.push_rx({0x00, 0x7A, 0x13, 0xFF});
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied_();
}

TEST_F(AirtonSerialFrameTest, FrameSplitAcrossLoops) {
  const auto frame = a3_frame();
  this->uart_.rx.assign(frame.begin(), frame.begin() + 10);
  this->sut_.loop();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  this->uart_.rx.assign(frame.begin() + 10, frame.end());
  this->sut_.loop();
  this->expect_a3_applied_();
}

TEST_F(AirtonSerialFrameTest, BadCrcDropped) {
  auto frame = a3_frame();
  frame[33] ^= 0xFF;
  this->uart_.push_rx(frame);
  this->sut_.loop();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);

  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied_();
}

TEST_F(AirtonSerialFrameTest, BadLengthRejected) {
  this->uart_.push_rx({0x7A, 0x7A, 0xD5, 0x21, 0x05});
  this->uart_.push_rx({0x7A, 0x7A, 0xD5, 0x21, MAX_FRAME_LEN + 1});
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied_();
}

// The rejected header holds the start of the real frame: 7A 7A FF, then 7A 7A whose second byte reads as a
// length of 0x7A.
TEST_F(AirtonSerialFrameTest, ResyncInsideRejectedHeader) {
  const auto frame = a3_frame();
  this->uart_.push_rx({0x7A, 0x7A, 0xFF});
  this->uart_.push_rx(frame);
  this->sut_.loop();
  this->expect_a3_applied_();
}

// Without the start check, the 34 declared bytes would swallow the head of the next frame.
TEST_F(AirtonSerialFrameTest, TruncatedFrameThenValidFrame) {
  const auto frame = a3_frame();
  this->uart_.rx.assign(frame.begin(), frame.begin() + 20);
  this->uart_.push_rx(frame);
  this->sut_.loop();
  this->expect_a3_applied_();
}

TEST_F(AirtonSerialFrameTest, NothingSentBeforeFirstState) {
  EXPECT_FALSE(this->sut_.set_flag(FLAG_ECO, true));
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_HEAT).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
}

// Bytes produced by the original driver for the same state.
TEST_F(AirtonSerialFrameTest, SetFlagSendsA1AfterState) {
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  EXPECT_TRUE(this->sut_.set_flag(FLAG_ECO, true));
  const std::vector<uint8_t> expected = {0x7A, 0x7A, 0x21, 0xD5, 0x18, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                         0x59, 0x08, 0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x9B};
  EXPECT_EQ(this->uart_.tx, expected);
}

// Shorter than its opcode and CRC, so it is not a frame even with a valid CRC.
TEST_F(AirtonSerialFrameTest, ShortFrameRejected) {
  std::vector<uint8_t> short_frame = {0x7A, 0x7A, 0xD5, 0x21, 7, 0x00, 0x00};
  set_crc(short_frame);
  this->uart_.push_rx(short_frame);
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied_();
}

// A corrupt length that is in range waits for bytes that never come; the gap releases the frame behind it.
TEST_F(AirtonSerialFrameTest, StalledPartialFrameReleasedAfterGap) {
  this->uart_.push_rx({0x7A, 0x7A, 0xD5, 0x21, 0x40});
  this->uart_.push_rx(a3_frame());
  this->loop_at_(1000);
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  this->loop_at_(1049);
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  this->loop_at_(1050);
  this->expect_a3_applied_();
}

TEST_F(AirtonSerialFrameTest, TargetTemperatureRoundedAndClamped) {
  this->receive_state_();
  this->sut_.make_call().set_target_temperature(35.0f).perform();
  EXPECT_FLOAT_EQ(this->sut_.target_temperature, 31.0f);
  EXPECT_EQ(this->sent_(13), 31 - 16);
  this->sut_.make_call().set_target_temperature(16.5f).perform();
  EXPECT_FLOAT_EQ(this->sut_.target_temperature, 17.0f);
  EXPECT_EQ(this->sent_(13), 17 - 16);
  this->sut_.make_call().set_target_temperature(10.0f).perform();
  EXPECT_FLOAT_EQ(this->sut_.target_temperature, 16.0f);
  EXPECT_EQ(this->sent_(13), 0);
}

struct SwingCase {
  climate::ClimateSwingMode mode;
  uint8_t code;
};

constexpr SwingCase SWING_CASES[] = {
    {climate::CLIMATE_SWING_OFF, 0x00},
    {climate::CLIMATE_SWING_VERTICAL, 0x01},
    {climate::CLIMATE_SWING_HORIZONTAL, 0x10},
    {climate::CLIMATE_SWING_BOTH, 0x11},
};

TEST_F(AirtonSerialFrameTest, SwingDecoded) {
  for (const auto &c : SWING_CASES) {
    this->receive_state_(c.code);
    EXPECT_EQ(this->sut_.swing_mode, c.mode) << "code " << static_cast<int>(c.code);
  }
}

TEST_F(AirtonSerialFrameTest, SwingEncoded) {
  for (const auto &from : SWING_CASES) {
    for (const auto &to : SWING_CASES) {
      this->receive_state_(from.code);
      this->sut_.make_call().set_swing_mode(to.mode).perform();
      EXPECT_EQ(this->sent_(14), to.code)
          << "from " << static_cast<int>(from.code) << " to " << static_cast<int>(to.code);
    }
  }
}

// An unrelated change sends the swing byte back as the unit reported it.
TEST_F(AirtonSerialFrameTest, SwingKeptOnOtherChanges) {
  this->receive_state_(0x10);
  this->sut_.set_flag(FLAG_ECO, true);
  EXPECT_EQ(this->sent_(14), 0x10);
}

// Quiet mutes the current speed; a switch toggle keeps that speed.
TEST_F(AirtonSerialFrameTest, QuietKeepsFanSpeed) {
  this->receive_state_(0, 0x19, (24 - 16) | 0x40);
  ASSERT_TRUE(this->sut_.fan_mode.has_value());
  EXPECT_EQ(*this->sut_.fan_mode, climate::CLIMATE_FAN_QUIET);
  this->sut_.set_flag(FLAG_ECO, true);
  EXPECT_EQ(this->sent_(12), 0x19);
  EXPECT_EQ(this->sent_(13), (24 - 16) | 0x40);
}

// Turned off from cool, the unit reports the power bit clear and keeps the cool mode bits.
TEST_F(AirtonSerialFrameTest, OffFromCoolStaysOff) {
  this->receive_state_();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_COOL);
  this->receive_state_(0, 0x51);
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  this->sut_.set_flag(FLAG_DISPLAY, false);
  EXPECT_EQ(this->sent_(12), 0x51);
}

TEST_F(AirtonSerialFrameTest, TurnOffFromHeat) {
  this->receive_state_(0, 0x5C);
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_HEAT);
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(this->sent_(12), 0x54);
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  EXPECT_EQ(this->sent_(12), 0x59);
}

// Super is the top bit of the fan nibble, reported with the speed it runs at.
TEST_F(AirtonSerialFrameTest, SuperFanIsStrong) {
  this->receive_state_(0, 0xD9);
  EXPECT_EQ(this->sut_.get_custom_fan_mode(), "strong");
  this->sut_.set_flag(FLAG_ECO, true);
  EXPECT_EQ(this->sent_(12), 0xD9);
}

TEST_F(AirtonSerialFrameTest, SuperKeepsReportedSpeed) {
  // Super on top of low (0x9) stays low instead of becoming speed 5
  this->receive_state_(0, 0x99);
  EXPECT_EQ(this->sut_.get_custom_fan_mode(), "strong");
  this->sut_.set_flag(FLAG_ECO, true);
  EXPECT_EQ(this->sent_(12), 0x99);
}

}  // namespace esphome::airton_serial::testing
