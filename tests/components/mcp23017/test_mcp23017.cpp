#include <gtest/gtest.h>

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
    for (size_t i = 1; i < write_count; i++, reg++) {
      this->writes.emplace_back(reg, write_buffer[i]);
      this->regs[alias(reg)] = write_buffer[i];
    }
    for (size_t i = 0; i < read_count; i++, reg++)
      read_buffer[i] = this->regs[alias(reg)];
    return i2c::ERROR_OK;
  }

  uint8_t &reg(uint8_t reg) { return this->regs[alias(reg)]; }

  uint8_t regs[0x16]{};
  Writes writes;
  bool fail{false};

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

TEST_F(MCP23017Test, SetupReadFailureMarksFailedAndWritesNothing) {
  this->bus_.fail = true;
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(MCP23017Test, SetupWithoutFlagsOnlyResetsInputPolarity) {
  this->bus_.reg(MCP23X17_IPOLA) = 0xFF;
  this->bus_.reg(MCP23X17_IPOLB) = 0x0F;
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.writes, (Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}}));
  EXPECT_EQ(this->bus_.reg(MCP23X17_IOCONA), 0x00);
}

TEST_F(MCP23017Test, SetupOpenDrainSetsOdrOnBothPorts) {
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());
  EXPECT_EQ(
      this->bus_.writes,
      (Writes{
          {MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, IOCON_ODR}, {MCP23X17_IOCONB, IOCON_ODR}}));
}

TEST_F(MCP23017Test, SetupInterruptPinSetsMirror) {
  this->int_pin_.set_inverted(true);  // INT released
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.writes, (Writes{{MCP23X17_IPOLA, 0x00},
                                       {MCP23X17_IPOLB, 0x00},
                                       {MCP23X17_IOCONA, IOCON_MIRROR},
                                       {MCP23X17_IOCONB, IOCON_MIRROR}}));
}

TEST_F(MCP23017Test, SetupOpenDrainAndInterruptPinKeepOtherIoconBits) {
  this->bus_.reg(MCP23X17_IOCONA) = IOCON_SEQOP;
  this->int_pin_.set_inverted(true);
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.set_open_drain_ints(true);
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());
  constexpr uint8_t expected = IOCON_SEQOP | IOCON_MIRROR | IOCON_ODR;
  EXPECT_EQ(
      this->bus_.writes,
      (Writes{
          {MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}, {MCP23X17_IOCONA, expected}, {MCP23X17_IOCONB, expected}}));
}

// Current behaviour: bits left by an earlier config stay set until the chip loses power.
TEST_F(MCP23017Test, SetupLeavesStaleOdrAndMirrorSet) {
  this->bus_.reg(MCP23X17_IOCONA) = IOCON_MIRROR | IOCON_ODR;
  this->component_.setup();
  ASSERT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.writes, (Writes{{MCP23X17_IPOLA, 0x00}, {MCP23X17_IPOLB, 0x00}}));
  EXPECT_EQ(this->bus_.reg(MCP23X17_IOCONA), IOCON_MIRROR | IOCON_ODR);
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
