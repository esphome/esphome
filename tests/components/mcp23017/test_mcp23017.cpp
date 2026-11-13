#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "esphome/components/mcp23017/mcp23017.h"

namespace esphome::mcp23017::testing {

using mcp23x17_base::MCP23X17_IOCONA;
using mcp23x17_base::MCP23X17_IOCONB;
using mcp23x17_base::MCP23X17_IPOLA;
using mcp23x17_base::MCP23X17_IPOLB;
using Writes = std::vector<std::pair<uint8_t, uint8_t>>;

static constexpr uint8_t IOCON_MIRROR = 0x40;
static constexpr uint8_t IOCON_SEQOP = 0x20;
static constexpr uint8_t IOCON_ODR = 0x04;

static Writes sorted(Writes writes) {
  std::sort(writes.begin(), writes.end());
  return writes;
}

/// Register file of a chip in BANK=0 mode, where IOCONA and IOCONB are the same register.
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
      this->regs[alias(reg)] = write_buffer[i];
    }
    for (size_t i = 0; i < read_count; i++, reg++)
      read_buffer[i] = this->regs[alias(reg)];
    return i2c::ERROR_OK;
  }

  uint8_t &register_value(uint8_t reg) { return this->regs[alias(reg)]; }

  uint8_t regs[0x16]{};
  Writes writes;
  bool fail{false};
  // Fail only reads or only writes that start at this register address
  int fail_read_reg{-1};
  int fail_write_reg{-1};

 protected:
  static uint8_t alias(uint8_t reg) { return reg == MCP23X17_IOCONB ? MCP23X17_IOCONA : reg; }
};

class MCP23017Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(FakeBus::ADDRESS);
    this->component_.set_open_drain_ints(false);
  }

  FakeBus bus_;
  MCP23017 component_;
  InternalGPIOPin int_pin_;
};

TEST_F(MCP23017Test, SetupIoconaReadFailure) {
  this->bus_.fail_read_reg = MCP23X17_IOCONA;
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(MCP23017Test, SetupIoconaWriteFailure) {
  this->bus_.fail_write_reg = MCP23X17_IOCONA;
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes), sorted(Writes{}));
}

TEST_F(MCP23017Test, SetupWithoutFlags) {
  this->bus_.register_value(MCP23X17_IPOLA) = 0xFF;
  this->bus_.register_value(MCP23X17_IPOLB) = 0x0F;
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes),
            sorted(Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, 0x00}}));
}

TEST_F(MCP23017Test, SetupOpenDrain) {
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes),
            sorted(Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, IOCON_ODR}}));
}

TEST_F(MCP23017Test, SetupInterruptPin) {
  this->int_pin_.set_inverted(true);  // INT released
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes),
            sorted(Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, IOCON_MIRROR}}));
}

TEST_F(MCP23017Test, SetupOpenDrainAndInterruptPin) {
  this->bus_.register_value(MCP23X17_IOCONA) = IOCON_SEQOP;
  this->int_pin_.set_inverted(true);  // INT released
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  constexpr uint8_t expected = IOCON_SEQOP | IOCON_MIRROR | IOCON_ODR;
  EXPECT_EQ(sorted(this->bus_.writes),
            sorted(Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, expected}}));
}

TEST_F(MCP23017Test, SetupStaleOdrAndMirror) {
  this->bus_.register_value(MCP23X17_IOCONA) = IOCON_SEQOP | IOCON_MIRROR | IOCON_ODR;
  this->component_.setup();
  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(sorted(this->bus_.writes),
            sorted(Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, IOCON_SEQOP}}));
  EXPECT_EQ(this->bus_.register_value(MCP23X17_IOCONA), IOCON_SEQOP);
}

TEST_F(MCP23017Test, ReadWarningClearsAfterSuccessfulRead) {
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());

  // One read per port: GPIOA (pin 0) and GPIOB (pin 8)
  for (uint8_t pin : {0, 8}) {
    SCOPED_TRACE(int(pin));
    this->bus_.fail = true;
    this->component_.digital_read(pin);
    EXPECT_TRUE(this->component_.status_has_warning());

    this->bus_.fail = false;
    this->component_.digital_read(pin);
    EXPECT_FALSE(this->component_.status_has_warning());
  }
}

}  // namespace esphome::mcp23017::testing
