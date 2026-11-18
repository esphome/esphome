#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/components/i2c/i2c.h"

namespace esphome::i2c::testing {

static constexpr uint8_t ADDRESS = 0x40;
static constexpr uint8_t REG = 0x12;

class RegisterBus final : public I2CBus {
 public:
  std::array<uint8_t, 256> regs{};
  std::vector<uint8_t> writes;
  bool fail_read{false};
  bool fail_write{false};

  ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in, size_t in_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (out_size == 0)
      return ERROR_TIMEOUT;
    const uint8_t reg = out[0];
    if (in_size > 0) {
      if (this->fail_read)
        return ERROR_TIMEOUT;
      for (size_t i = 0; i < in_size; i++)
        in[i] = this->regs[reg + i];
      return ERROR_OK;
    }
    if (this->fail_write)
      return ERROR_TIMEOUT;
    this->writes.push_back(reg);
    for (size_t i = 1; i < out_size; i++)
      this->regs[reg + i - 1] = out[i];
    return ERROR_OK;
  }
};

class UpdateRegisterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(ADDRESS);
  }

  RegisterBus bus_;
  I2CDevice device_;
};

TEST_F(UpdateRegisterTest, ReplacesOnlyMaskedBits) {
  this->bus_.regs[REG] = 0b1010'0101;
  EXPECT_TRUE(this->device_.update_register(REG, 0b0000'1111, 0b0000'0011));
  EXPECT_EQ(this->bus_.regs[REG], 0b1010'0011);
}

TEST_F(UpdateRegisterTest, IgnoresValueBitsOutsideMask) {
  this->bus_.regs[REG] = 0x00;
  EXPECT_TRUE(this->device_.update_register(REG, 0x0F, 0xFF));
  EXPECT_EQ(this->bus_.regs[REG], 0x0F);
}

TEST_F(UpdateRegisterTest, WritesEvenWhenUnchanged) {
  this->bus_.regs[REG] = 0x3C;
  EXPECT_TRUE(this->device_.update_register(REG, 0x0C, 0x0C));
  EXPECT_EQ(this->bus_.regs[REG], 0x3C);
  EXPECT_EQ(this->bus_.writes, std::vector<uint8_t>{REG});
}

TEST_F(UpdateRegisterTest, FailedReadWritesNothing) {
  this->bus_.fail_read = true;
  EXPECT_FALSE(this->device_.update_register(REG, 0xFF, 0x01));
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(UpdateRegisterTest, FailedWriteReturnsFalse) {
  this->bus_.fail_write = true;
  EXPECT_FALSE(this->device_.update_register(REG, 0xFF, 0x01));
}

}  // namespace esphome::i2c::testing
