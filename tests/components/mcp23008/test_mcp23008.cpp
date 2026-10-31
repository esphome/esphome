#include <gtest/gtest.h>

#include "esphome/components/mcp23008/mcp23008.h"

namespace esphome::mcp23008::testing {

class FakeBus : public i2c::I2CBus {
 public:
  static constexpr uint8_t ADDRESS = 0x20;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_TIMEOUT;
    for (size_t i = 0; i < read_count; i++)
      read_buffer[i] = 0x00;
    return i2c::ERROR_OK;
  }

  bool fail{false};
};

class MCP23008Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(FakeBus::ADDRESS);
  }

  FakeBus bus_;
  MCP23008 component_;
};

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
