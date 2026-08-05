#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <vector>

#include "esphome/components/as734x/as7341.h"
#include "esphome/components/as734x/as7343.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::as734x::testing {

namespace {

class FakeI2CBus : public i2c::I2CBus {
 public:
  std::map<uint8_t, std::vector<uint8_t>> registers;

  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    if (read_count == 0) {
      return i2c::ERROR_OK;  // a plain write, nothing to answer
    }
    if (write_count != 1) {
      return i2c::ERROR_INVALID_ARGUMENT;
    }
    const auto it = this->registers.find(write_buffer[0]);
    if (it == this->registers.end() || it->second.size() < read_count) {
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    std::memcpy(read_buffer, it->second.data(), read_count);
    return i2c::ERROR_OK;
  }
};

constexpr uint8_t ASTATUS = 0x94;
constexpr uint8_t DATA_0 = 0x95;

std::vector<uint8_t> little_endian_frame(const std::vector<uint16_t> &words) {
  std::vector<uint8_t> bytes;
  for (uint16_t word : words) {
    bytes.push_back(static_cast<uint8_t>(word & 0xFF));
    bytes.push_back(static_cast<uint8_t>(word >> 8));
  }
  return bytes;
}

}  // namespace

class ChannelMappingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x39);
    this->bus_.registers[ASTATUS] = {0x00};
  }

  void set_frame_(const std::vector<uint16_t> &words) { this->bus_.registers[DATA_0] = little_endian_frame(words); }

  FakeI2CBus bus_;
  i2c::I2CDevice device_;
};

TEST_F(ChannelMappingTest, As7341PlacesEveryBandAtItsPublishedIndex) {
  AS7341 chip(&this->device_);
  ChannelValuesUint16 values{};
  bool saturated = false;

  this->set_frame_({101, 102, 103, 104, 900, 900});
  ASSERT_TRUE(chip.read_channels(0, values, saturated));

  this->set_frame_({105, 106, 107, 108, 5555, 8888});
  ASSERT_TRUE(chip.read_channels(1, values, saturated));

  EXPECT_EQ(values[0], 101);  // F1
  EXPECT_EQ(values[1], 102);  // F2
  EXPECT_EQ(values[2], 103);  // F3
  EXPECT_EQ(values[3], 104);  // F4
  EXPECT_EQ(values[4], 105);  // F5
  EXPECT_EQ(values[5], 106);  // F6
  EXPECT_EQ(values[6], 107);  // F7
  EXPECT_EQ(values[7], 108);  // F8
  EXPECT_EQ(values[8], 8888) << "index 8 is NIR, which the SMUX routes to ADC5";
  EXPECT_EQ(values[9], 5555) << "index 9 is CLEAR, which the SMUX routes to ADC4";
}

TEST_F(ChannelMappingTest, As7341ReadsDataRegistersLowByteFirst) {
  AS7341 chip(&this->device_);
  ChannelValuesUint16 values{};
  bool saturated = false;

  this->set_frame_({0x012C, 0, 0, 0, 0, 0});  // 300, which byte-reversed would read 11265
  ASSERT_TRUE(chip.read_channels(0, values, saturated));
  EXPECT_EQ(values[0], 300);
}

TEST_F(ChannelMappingTest, As7343PlacesEveryBandAtItsPublishedIndex) {
  AS7343 chip(&this->device_);
  ChannelValuesUint16 values{};
  bool saturated = false;

  this->bus_.registers[0x93] = {0x00};  // STATUS
  // auto_smux routes three cycles of six ADCs. Each slot carries its own wavelength so a band that
  // reads from the wrong slot reports another band's number.
  const std::vector<uint16_t> frame{
      450, 555, 600, 855, 1000, 9001,  // cycle 1: FZ FY FXL NIR CLEAR FD
      425, 475, 515, 640, 1100, 9002,  // cycle 2: F2 F3 F4 F6 CLEAR FD
      405, 690, 745, 550, 0,    9003,  // cycle 3: F1 F7 F8 F5 (clear reads zero here) FD
  };
  this->set_frame_(frame);
  ASSERT_TRUE(chip.read_channels(0, values, saturated));

  EXPECT_EQ(values[0], 405) << "F1";
  EXPECT_EQ(values[1], 425) << "F2";
  EXPECT_EQ(values[2], 450) << "FZ";
  EXPECT_EQ(values[3], 475) << "F3";
  EXPECT_EQ(values[4], 515) << "F4";
  EXPECT_EQ(values[5], 555) << "FY";
  EXPECT_EQ(values[6], 550) << "F5 is the fourth ADC of cycle 3, not the second";
  EXPECT_EQ(values[7], 600) << "FXL";
  EXPECT_EQ(values[8], 640) << "F6";
  EXPECT_EQ(values[9], 690) << "F7 is the second ADC of cycle 3";
  EXPECT_EQ(values[10], 745) << "F8 is the third ADC of cycle 3";
  EXPECT_EQ(values[11], 855) << "NIR";
  EXPECT_EQ(values[12], 1050) << "CLEAR is the average of the two cycles that route it";
}

}  // namespace esphome::as734x::testing
