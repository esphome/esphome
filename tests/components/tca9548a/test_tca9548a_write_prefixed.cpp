#include <gtest/gtest.h>

#include <vector>
#include "esphome/components/tca9548a/tca9548a.h"

namespace esphome::tca9548a::testing {

static constexpr uint8_t MUX_ADDRESS = 0x70;
static constexpr uint8_t DEVICE_ADDRESS = 0x5A;

// One upstream call; a prefixed write has a non-empty prefix
struct Call {
  uint8_t address;
  std::vector<uint8_t> prefix;
  std::vector<uint8_t> data;
  size_t read_count;
  bool operator==(const Call &other) const = default;
};

class RecordingBus final : public i2c::I2CBus {
 public:
  std::vector<Call> calls;
  const uint8_t *data_ptr{nullptr};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    this->calls.push_back({address, {}, {write_buffer, write_buffer + write_count}, read_count});
    return i2c::ERROR_OK;
  }

  // A frequency switch is recorded as a call to address 0 carrying the frequency
  i2c::ErrorCode switch_frequency(uint32_t frequency) override {
    this->calls.push_back({0, {}, {}, frequency});
    this->frequency = frequency;
    return i2c::ERROR_OK;
  }
  uint32_t get_frequency() const override { return this->frequency; }
  uint32_t frequency{100000};

  i2c::ErrorCode write_prefixed(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *data,
                                size_t len) override {
    this->calls.push_back({address, {prefix, prefix + prefix_len}, {data, data + len}, 0});
    this->data_ptr = data;
    return i2c::ERROR_OK;
  }
};

static const Call SELECT_CHANNEL_3{MUX_ADDRESS, {}, {1 << 3}, 0};
static const Call DESELECT{MUX_ADDRESS, {}, {0}, 0};
static Call switch_to(uint32_t frequency) { return {0, {}, {}, frequency}; }

class TCA9548AWritePrefixedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->mux_.set_i2c_bus(&this->bus_);
    this->mux_.set_i2c_address(MUX_ADDRESS);
    this->channel_.set_parent(&this->mux_);
    this->channel_.set_channel(3);
    this->device_.set_i2c_bus(&this->channel_);
    this->device_.set_i2c_address(DEVICE_ADDRESS);
  }

  RecordingBus bus_;
  TCA9548AComponent mux_;
  TCA9548AChannel channel_;
  i2c::I2CDevice device_;
};

TEST_F(TCA9548AWritePrefixedTest, RegisterWriteReachesTheUpstreamBusInTwoParts) {
  const uint8_t data[3] = {1, 2, 3};
  EXPECT_EQ(this->device_.write_register16(0x1234, data, sizeof(data)), i2c::ERROR_OK);
  const std::vector<Call> expected{SELECT_CHANNEL_3, {DEVICE_ADDRESS, {0x12, 0x34}, {1, 2, 3}, 0}, DESELECT};
  EXPECT_EQ(this->bus_.calls, expected);
  // The caller's buffer is passed through, not a joined copy
  EXPECT_EQ(this->bus_.data_ptr, data);
}

TEST_F(TCA9548AWritePrefixedTest, ReadsAndPlainWritesStillUseWriteReadv) {
  uint8_t out[2];
  EXPECT_EQ(this->device_.read_register(0x10, out, sizeof(out)), i2c::ERROR_OK);
  const uint8_t value = 7;
  EXPECT_EQ(this->device_.write(&value, 1), i2c::ERROR_OK);
  const std::vector<Call> expected{SELECT_CHANNEL_3, {DEVICE_ADDRESS, {}, {0x10}, 2}, DESELECT,
                                   SELECT_CHANNEL_3, {DEVICE_ADDRESS, {}, {7}, 0},    DESELECT};
  EXPECT_EQ(this->bus_.calls, expected);
}

TEST_F(TCA9548AWritePrefixedTest, RegisterWriteAtAPortFrequencyStaysInTwoParts) {
  // The mux addresses at 100 kHz (the bus), the device runs at 400 kHz
  this->channel_.set_frequency(400000);
  const uint8_t data[3] = {1, 2, 3};
  EXPECT_EQ(this->device_.write_register(0x10, data, sizeof(data)), i2c::ERROR_OK);
  const std::vector<Call> expected{
      switch_to(100000), SELECT_CHANNEL_3, switch_to(400000), {DEVICE_ADDRESS, {0x10}, {1, 2, 3}, 0},
      switch_to(100000), DESELECT,         switch_to(100000)};
  EXPECT_EQ(this->bus_.calls, expected);
  EXPECT_EQ(this->bus_.data_ptr, data);
}

TEST_F(TCA9548AWritePrefixedTest, RegisterWriteAtTheMuxFrequencyStaysInTwoParts) {
  this->mux_.set_frequency(400000);
  const uint8_t data[2] = {4, 5};
  EXPECT_EQ(this->device_.write_register(0x20, data, sizeof(data)), i2c::ERROR_OK);
  const std::vector<Call> expected{
      switch_to(400000), SELECT_CHANNEL_3, {DEVICE_ADDRESS, {0x20}, {4, 5}, 0}, DESELECT, switch_to(100000)};
  EXPECT_EQ(this->bus_.calls, expected);
  EXPECT_EQ(this->bus_.data_ptr, data);
}

}  // namespace esphome::tca9548a::testing
