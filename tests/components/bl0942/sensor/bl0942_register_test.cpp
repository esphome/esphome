#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/bl0942/bl0942.h"
#include "../common.h"

namespace esphome::bl0942::testing {

namespace {

class BL0942RegisterTest : public ::testing::Test {
 protected:
  void SetUp() override { this->sut_.set_uart_parent(&this->uart_); }

  FakeUART uart_;
  BL0942 sut_;
};

// setup() writes four registers, then reads back the mode register to check the writes took.
TEST_F(BL0942RegisterTest, SetupFramesAndModeReadback) {
  // Mode 0x8F read back: 0x58 + 0x19 + 0x8F = 0x100, so the checksum wraps to 0x00 ^ 0xFF.
  this->uart_.rx = {0x8F, 0x00, 0x00, 0xFF};
  this->sut_.setup();
  const std::vector<uint8_t> expected{
      0xA8, 0x1D, 0x55, 0x00, 0x00, 0xE5,  // USR_WRPROT = 0x55
      0xA8, 0x19, 0x8F, 0x00, 0x00, 0xAF,  // MODE = 0x8F
      0xA8, 0x1D, 0x00, 0x00, 0x00, 0x3A,  // USR_WRPROT = 0
      0x58, 0x19,                          // read MODE
  };
  EXPECT_EQ(this->uart_.tx, expected);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(BL0942RegisterTest, SetupWithAddressResetAnd60Hz) {
  this->sut_.set_address(2);
  this->sut_.set_reset(true);
  this->sut_.set_line_freq(LINE_FREQUENCY_60HZ);
  this->uart_.rx = {0xAF, 0x00, 0x00, 0xDD};
  this->sut_.setup();
  const std::vector<uint8_t> expected{
      0xAA, 0x1D, 0x55, 0x00, 0x00, 0xE3,  // USR_WRPROT = 0x55
      0xAA, 0x1C, 0x5A, 0x5A, 0x5A, 0x2B,  // SOFT_RESET = 0x5A5A5A
      0xAA, 0x19, 0xAF, 0x00, 0x00, 0x8D,  // MODE = 0xAF
      0xAA, 0x1D, 0x00, 0x00, 0x00, 0x38,  // USR_WRPROT = 0
      0x5A, 0x19,                          // read MODE
  };
  EXPECT_EQ(this->uart_.tx, expected);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(BL0942RegisterTest, ReadbackWithBadChecksumWarns) {
  this->uart_.rx = {0x8F, 0x00, 0x00, 0xFE};
  this->sut_.setup();
  EXPECT_TRUE(this->sut_.status_has_warning());
}

// The register value is little endian: 0x8F in the top byte is 0x8F0000, not the mode that was written.
TEST_F(BL0942RegisterTest, ReadbackIsLittleEndian) {
  this->uart_.rx = {0x00, 0x00, 0x8F, 0xFF};
  this->sut_.setup();
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(BL0942RegisterTest, ShortReadbackWarns) {
  this->uart_.rx = {0x8F, 0x00};
  this->sut_.setup();
  EXPECT_TRUE(this->sut_.status_has_warning());
}

}  // namespace

}  // namespace esphome::bl0942::testing
