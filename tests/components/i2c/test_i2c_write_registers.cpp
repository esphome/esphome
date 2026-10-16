#include <gtest/gtest.h>

#include <array>
#include <utility>
#include <vector>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome::i2c::testing {

class RecordingBus : public I2CBus {
 public:
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    EXPECT_EQ(address, 0x18);
    EXPECT_EQ(write_count, 2u);
    EXPECT_EQ(read_count, 0u);
    if (this->writes.size() == this->fail_at)
      return ERROR_NOT_ACKNOWLEDGED;
    this->writes.emplace_back(write_buffer[0], write_buffer[1]);
    return ERROR_OK;
  }

  std::vector<std::pair<uint8_t, uint8_t>> writes;
  size_t fail_at{SIZE_MAX};
};

static constexpr I2CRegValue INIT[] PROGMEM = {{0x01, 0x3A}, {0x00, 0x80}, {0xF9, 0x00}, {0x01, 0x00}};

class I2CWriteRegistersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x18);
  }
  RecordingBus bus_;
  I2CDevice device_;
};

TEST_F(I2CWriteRegistersTest, WritesEveryEntryInOrder) {
  ASSERT_TRUE(this->device_.write_registers(INIT));
  const std::vector<std::pair<uint8_t, uint8_t>> expected{{0x01, 0x3A}, {0x00, 0x80}, {0xF9, 0x00}, {0x01, 0x00}};
  EXPECT_EQ(this->bus_.writes, expected);
}

TEST_F(I2CWriteRegistersTest, StopsAtFirstFailure) {
  this->bus_.fail_at = 2;
  EXPECT_FALSE(this->device_.write_registers(INIT));
  const std::vector<std::pair<uint8_t, uint8_t>> expected{{0x01, 0x3A}, {0x00, 0x80}};
  EXPECT_EQ(this->bus_.writes, expected);
}

TEST_F(I2CWriteRegistersTest, PointerAndCountOverload) {
  ASSERT_TRUE(this->device_.write_registers(&INIT[1], 2));
  const std::vector<std::pair<uint8_t, uint8_t>> expected{{0x00, 0x80}, {0xF9, 0x00}};
  EXPECT_EQ(this->bus_.writes, expected);
}

TEST_F(I2CWriteRegistersTest, StdArrayOverload) {
  static constexpr std::array<I2CRegValue, 2> TABLE{{{0x10, 0x01}, {0x11, 0x02}}};
  ASSERT_TRUE(this->device_.write_registers(TABLE));
  const std::vector<std::pair<uint8_t, uint8_t>> expected{{0x10, 0x01}, {0x11, 0x02}};
  EXPECT_EQ(this->bus_.writes, expected);
}

TEST_F(I2CWriteRegistersTest, EmptyTableSucceeds) {
  EXPECT_TRUE(this->device_.write_registers(INIT, 0));
  EXPECT_TRUE(this->bus_.writes.empty());
}

}  // namespace esphome::i2c::testing
