#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "esphome/components/mcp23008/mcp23008.h"

namespace esphome::mcp23008::testing {

using mcp23x08_base::MCP23X08_IOCON;
using Writes = std::vector<std::pair<uint8_t, uint8_t>>;

static constexpr uint8_t IOCON_SEQOP = 0x20;
static constexpr uint8_t IOCON_ODR = 0x04;

static Writes sorted(Writes writes) {
  std::sort(writes.begin(), writes.end());
  return writes;
}

class FakeBus : public i2c::I2CBus {
 public:
  static constexpr uint8_t ADDRESS = 0x20;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_TIMEOUT;
    if (write_count == 0)
      return i2c::ERROR_OK;
    uint8_t reg = write_buffer[0];
    if (reg == (write_count > 1 ? this->fail_write_reg : this->fail_read_reg))
      return i2c::ERROR_TIMEOUT;
    for (size_t i = 1; i < write_count; i++, reg++) {
      this->writes.emplace_back(reg, write_buffer[i]);
      this->regs[reg] = write_buffer[i];
    }
    for (size_t i = 0; i < read_count; i++, reg++)
      read_buffer[i] = this->regs[reg];
    return i2c::ERROR_OK;
  }

  uint8_t regs[0x0B]{};
  Writes writes;
  bool fail{false};
  // Fail only reads or only writes that start at this register address
  int fail_read_reg{-1};
  int fail_write_reg{-1};
};

class MCP23008Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(FakeBus::ADDRESS);
    this->component_.set_open_drain_ints(false);
  }

  FakeBus bus_;
  MCP23008 component_;
};

TEST_F(MCP23008Test, SetupIoconReadFailure) {
  this->bus_.fail_read_reg = MCP23X08_IOCON;
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(MCP23008Test, SetupIoconWriteFailure) {
  this->bus_.fail_write_reg = MCP23X08_IOCON;
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(MCP23008Test, SetupWithoutOpenDrain) {
  this->bus_.regs[MCP23X08_IOCON] = IOCON_SEQOP;
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes), sorted(Writes{{MCP23X08_IOCON, IOCON_SEQOP}}));
}

TEST_F(MCP23008Test, SetupOpenDrain) {
  this->bus_.regs[MCP23X08_IOCON] = IOCON_SEQOP;
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes), sorted(Writes{{MCP23X08_IOCON, IOCON_SEQOP | IOCON_ODR}}));
}

TEST_F(MCP23008Test, SetupStaleOdr) {
  this->bus_.regs[MCP23X08_IOCON] = IOCON_SEQOP | IOCON_ODR;
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes), sorted(Writes{{MCP23X08_IOCON, IOCON_SEQOP}}));
  EXPECT_EQ(this->bus_.regs[MCP23X08_IOCON], IOCON_SEQOP);
}

TEST_F(MCP23008Test, ReadWarningClearsAfterSuccessfulRead) {
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());

  this->bus_.fail = true;
  this->component_.digital_read(0);
  EXPECT_TRUE(this->component_.status_has_warning());

  this->bus_.fail = false;
  this->component_.digital_read(0);
  EXPECT_FALSE(this->component_.status_has_warning());
}

}  // namespace esphome::mcp23008::testing
