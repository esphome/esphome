#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/airton_serial/airton_serial_climate.h"
#include "esphome/core/helpers.h"
#include "../common.h"

namespace esphome::airton_serial::testing {

namespace {

// A3 state frame from the climate board: cool, fan high, 24 C setpoint, 22 C ambient.
std::vector<uint8_t> a3_frame() {
  std::vector<uint8_t> frame(34, 0);
  frame[0] = 0x7A;
  frame[1] = 0x7A;
  frame[2] = 0xD5;
  frame[3] = 0x21;
  frame[4] = 34;
  frame[7] = 0xA3;
  frame[10] = 22;
  frame[13] = 0x59;
  frame[14] = 24 - 16;
  frame[16] = 0x80;
  const uint16_t crc = crc16(frame.data(), frame.size() - 2);
  frame[32] = crc >> 8;
  frame[33] = crc & 0xFF;
  return frame;
}

class AirtonSerialFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.mode = climate::CLIMATE_MODE_OFF;
  }

  void expect_a3_applied() {
    EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(this->sut_.target_temperature, 24.0f);
    EXPECT_FLOAT_EQ(this->sut_.current_temperature, 22.0f);
    ASSERT_TRUE(this->sut_.fan_mode.has_value());
    EXPECT_EQ(*this->sut_.fan_mode, climate::CLIMATE_FAN_HIGH);
  }

  MockUARTComponent uart_;
  AirtonSerialClimate sut_;
};

}  // namespace

TEST_F(AirtonSerialFrameTest, ValidFrameAfterNoise) {
  this->uart_.push_rx({0x00, 0x7A, 0x13, 0xFF});
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied();
}

TEST_F(AirtonSerialFrameTest, FrameSplitAcrossLoops) {
  const auto frame = a3_frame();
  this->uart_.rx.assign(frame.begin(), frame.begin() + 10);
  this->sut_.loop();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
  this->uart_.rx.assign(frame.begin() + 10, frame.end());
  this->sut_.loop();
  this->expect_a3_applied();
}

TEST_F(AirtonSerialFrameTest, BadCrcDropped) {
  auto frame = a3_frame();
  frame[33] ^= 0xFF;
  this->uart_.push_rx(frame);
  this->sut_.loop();
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);

  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied();
}

TEST_F(AirtonSerialFrameTest, BadLengthRejected) {
  this->uart_.push_rx({0x7A, 0x7A, 0xD5, 0x21, 0x05});
  this->uart_.push_rx({0x7A, 0x7A, 0xD5, 0x21, MAX_FRAME_LEN + 1});
  this->uart_.push_rx(a3_frame());
  this->sut_.loop();
  this->expect_a3_applied();
}

// The rejected header holds the start of the real frame: 7A 7A FF, then 7A 7A whose second byte reads as a
// length of 0x7A.
TEST_F(AirtonSerialFrameTest, ResyncInsideRejectedHeader) {
  const auto frame = a3_frame();
  this->uart_.push_rx({0x7A, 0x7A, 0xFF});
  this->uart_.push_rx(frame);
  this->sut_.loop();
  this->expect_a3_applied();
}

// Without the start check, the 34 declared bytes would swallow the head of the next frame.
TEST_F(AirtonSerialFrameTest, TruncatedFrameThenValidFrame) {
  const auto frame = a3_frame();
  this->uart_.rx.assign(frame.begin(), frame.begin() + 20);
  this->uart_.push_rx(frame);
  this->sut_.loop();
  this->expect_a3_applied();
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

}  // namespace esphome::airton_serial::testing
