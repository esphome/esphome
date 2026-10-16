#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "esphome/components/i2c/i2c.h"

namespace esphome::i2c::testing {

class MemoryBus : public I2CBus {
 public:
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    if (this->fail)
      return ERROR_NOT_ACKNOWLEDGED;
    if (write_count == 0)
      return ERROR_INVALID_ARGUMENT;
    uint8_t reg = write_buffer[0];
    for (size_t i = 1; i < write_count; i++)
      this->memory[reg + i - 1] = write_buffer[i];
    for (size_t i = 0; i < read_count; i++)
      read_buffer[i] = this->memory[reg + i];
    this->last_write_count = write_count;
    return ERROR_OK;
  }

  std::array<uint8_t, 32> memory{};
  size_t last_write_count{0};
  bool fail{false};
};

class I2CLittleEndianTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x40);
  }
  MemoryBus bus_;
  I2CDevice device_;
};

TEST_F(I2CLittleEndianTest, ReadByte16LittleEndian) {
  this->bus_.memory[4] = 0x34;
  this->bus_.memory[5] = 0x12;
  uint16_t value = 0;
  ASSERT_TRUE(this->device_.read_byte_16_le(4, &value));
  EXPECT_EQ(value, 0x1234);
  ASSERT_TRUE(this->device_.read_byte_16(4, &value));
  EXPECT_EQ(value, 0x3412);
}

TEST_F(I2CLittleEndianTest, ReadBytes16LittleEndian) {
  static constexpr uint8_t RAW[] = {0x01, 0x02, 0xFE, 0xFF, 0x00, 0x80};
  std::memcpy(&this->bus_.memory[8], RAW, sizeof(RAW));
  uint16_t values[3]{};
  ASSERT_TRUE(this->device_.read_bytes_16_le(8, values, 3));
  EXPECT_EQ(values[0], 0x0201);
  EXPECT_EQ(static_cast<int16_t>(values[1]), -2);
  EXPECT_EQ(values[2], 0x8000);
}

TEST_F(I2CLittleEndianTest, ReadFailureReturnsFalse) {
  this->bus_.fail = true;
  uint16_t value = 0xBEEF;
  EXPECT_FALSE(this->device_.read_byte_16_le(0, &value));
  EXPECT_EQ(value, 0xBEEF);
}

TEST_F(I2CLittleEndianTest, WriteByte16LittleEndian) {
  ASSERT_TRUE(this->device_.write_byte_16_le(2, 0xA1B2));
  EXPECT_EQ(this->bus_.last_write_count, 3u);
  EXPECT_EQ(this->bus_.memory[2], 0xB2);
  EXPECT_EQ(this->bus_.memory[3], 0xA1);
  uint16_t value = 0;
  ASSERT_TRUE(this->device_.read_byte_16_le(2, &value));
  EXPECT_EQ(value, 0xA1B2);
}

TEST_F(I2CLittleEndianTest, WriteFailureReturnsFalse) {
  this->bus_.fail = true;
  EXPECT_FALSE(this->device_.write_byte_16_le(2, 0xA1B2));
}

}  // namespace esphome::i2c::testing
