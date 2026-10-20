#include <gtest/gtest.h>

#include <new>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"

namespace esphome::i2c::testing {

class IdBus : public I2CBus {
 public:
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    ++this->transactions;
    if (this->fail)
      return ERROR_NOT_ACKNOWLEDGED;
    if (write_count != 1 || read_count != 1)
      return ERROR_INVALID_ARGUMENT;
    this->last_register = write_buffer[0];
    read_buffer[0] = this->id;
    return ERROR_OK;
  }

  uint8_t id{0};
  uint8_t last_register{0};
  unsigned transactions{0};
  bool fail{false};
};

class ChipDevice : public Component, public I2CDevice {};

class I2CChipIdTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // The test main keeps App alive; this test replaces it with a fresh one it owns
    App.~Application();
    new (&App) Application();
    static char name[] = "i2c-chip-id-tests";
    static char friendly_name[] = "I2C chip ID tests";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, sizeof(friendly_name) - 1);
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x40);
  }
  IdBus bus_;
  ChipDevice device_;
};

TEST_F(I2CChipIdTest, MatchingIdPasses) {
  this->bus_.id = 0x33;
  EXPECT_TRUE(this->device_.check_chip_id(&this->device_, 0x0F, 0x33));
  EXPECT_EQ(this->bus_.last_register, 0x0F);
  EXPECT_EQ(this->bus_.transactions, 1u);
  EXPECT_FALSE(this->device_.is_failed());
}

TEST_F(I2CChipIdTest, WrongIdFails) {
  this->bus_.id = 0x32;
  EXPECT_FALSE(this->device_.check_chip_id(&this->device_, 0x0F, 0x33));
  EXPECT_TRUE(this->device_.is_failed());
}

TEST_F(I2CChipIdTest, ReadFailureFails) {
  this->bus_.fail = true;
  EXPECT_FALSE(this->device_.check_chip_id(&this->device_, 0x0F, 0x00));
  EXPECT_TRUE(this->device_.is_failed());
}

TEST_F(I2CChipIdTest, MaskAcceptsTwoIds) {
  // 0xB1 and 0xB3 differ only in bit 1
  for (uint8_t id : {0xB1, 0xB3}) {
    this->bus_.id = id;
    EXPECT_TRUE(this->device_.check_chip_id(&this->device_, 0x06, 0xB1, 0xFD));
  }
  EXPECT_FALSE(this->device_.is_failed());
  this->bus_.id = 0xB5;
  EXPECT_FALSE(this->device_.check_chip_id(&this->device_, 0x06, 0xB1, 0xFD));
  EXPECT_TRUE(this->device_.is_failed());
}

TEST_F(I2CChipIdTest, MaskIgnoresRevisionBits) {
  this->bus_.id = 0x27;
  EXPECT_TRUE(this->device_.check_chip_id(&this->device_, 0x92, 0x24, 0xFC));
  EXPECT_FALSE(this->device_.is_failed());
  this->bus_.id = 0x2B;
  EXPECT_FALSE(this->device_.check_chip_id(&this->device_, 0x92, 0x24, 0xFC));
  EXPECT_TRUE(this->device_.is_failed());
}

TEST_F(I2CChipIdTest, ExpectedBitsOutsideTheMaskAreIgnored) {
  this->bus_.id = 0x24;
  EXPECT_TRUE(this->device_.check_chip_id(&this->device_, 0x92, 0x27, 0xFC));
  EXPECT_FALSE(this->device_.is_failed());
}

}  // namespace esphome::i2c::testing
