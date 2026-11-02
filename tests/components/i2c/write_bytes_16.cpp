#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/i2c/i2c.h"

namespace esphome::i2c::testing {

class RecordingBus final : public I2CBus {
 public:
  std::vector<uint8_t> written;
  ErrorCode result{ERROR_OK};

  ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    this->written.assign(data, data + size);
    return this->result;
  }
};

class WriteBytes16Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x40);
  }

  RecordingBus bus_;
  I2CDevice device_;
};

TEST_F(WriteBytes16Test, WritesRegisterThenWordBigEndian) {
  EXPECT_TRUE(this->device_.write_byte_16(0x12, 0xABCD));
  EXPECT_EQ(this->bus_.written, (std::vector<uint8_t>{0x12, 0xAB, 0xCD}));
}

TEST_F(WriteBytes16Test, WritesEveryWordBigEndian) {
  const uint16_t words[3] = {0x0102, 0x0304, 0xFF00};
  EXPECT_TRUE(this->device_.write_bytes_16(0x7E, words, 3));
  EXPECT_EQ(this->bus_.written, (std::vector<uint8_t>{0x7E, 0x01, 0x02, 0x03, 0x04, 0xFF, 0x00}));
}

TEST_F(WriteBytes16Test, MoreWordsThanTheStackBufferHolds) {
  uint16_t words[12];
  std::vector<uint8_t> expected{0x01};
  for (uint16_t i = 0; i < 12; i++) {
    words[i] = 0x1000 + i;
    expected.push_back(0x10);
    expected.push_back(i);
  }
  EXPECT_TRUE(this->device_.write_bytes_16(0x01, words, 12));
  EXPECT_EQ(this->bus_.written, expected);
}

TEST_F(WriteBytes16Test, ReportsBusFailure) {
  this->bus_.result = ERROR_NOT_ACKNOWLEDGED;
  EXPECT_FALSE(this->device_.write_byte_16(0x12, 0x0001));
}

}  // namespace esphome::i2c::testing
