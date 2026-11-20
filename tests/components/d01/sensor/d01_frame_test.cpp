#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "esphome/components/d01/d01.h"
#include "../common.h"

namespace esphome::d01::testing {

namespace {

class D01FrameTest : public ::testing::Test {
 protected:
  void SetUp() override { this->sut_.set_uart_parent(&this->uart_); }

  FakeUART uart_;
  D01SensorComponent sut_;
};

// The checksum is the low 7 bits of the sum of the first three bytes.
TEST_F(D01FrameTest, PublishesConcentration) {
  this->uart_.rx = {0xA5, 0x12, 0x34, 0x6B};
  this->sut_.loop();
  EXPECT_FLOAT_EQ(this->sut_.state, 0x12 * 128 + 0x34);
}

TEST_F(D01FrameTest, ChecksumDropsCarry) {
  // 0xA5 + 0x7F + 0x7F = 0x1A3
  this->uart_.rx = {0xA5, 0x7F, 0x7F, 0x23};
  this->sut_.loop();
  EXPECT_FLOAT_EQ(this->sut_.state, 0x7F * 128 + 0x7F);
}

TEST_F(D01FrameTest, BadChecksumIsDropped) {
  this->uart_.rx = {0xA5, 0x12, 0x34, 0x6C};
  this->sut_.loop();
  EXPECT_TRUE(std::isnan(this->sut_.state));
  EXPECT_TRUE(this->uart_.rx.empty());
}

}  // namespace

}  // namespace esphome::d01::testing
