#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/mcp23017/mcp23017.h"

namespace esphome::mcp23017::testing {

class FakeBus : public i2c::I2CBus {
 public:
  static constexpr uint8_t ADDRESS = 0x20;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_TIMEOUT;
    if (read_count != 0)
      std::memset(read_buffer, 0, read_count);
    return i2c::ERROR_OK;
  }

  bool fail{false};
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
};

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
