#include <gtest/gtest.h>

#include <string>
#include <vector>
#include "esphome/components/tca9548a/tca9548a.h"

namespace esphome::tca9548a::testing {

static constexpr uint8_t MUX_ADDRESS = 0x70;
static constexpr uint8_t DEVICE_ADDRESS = 0x5A;

// Records every upstream call: "readv addr bytes..." or "prefixed addr prefix... | data..."
class RecordingBus final : public i2c::I2CBus {
 public:
  std::vector<std::string> calls;
  const uint8_t *data_ptr{nullptr};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    std::string call = "readv " + std::to_string(address);
    for (size_t i = 0; i < write_count; i++)
      call += " " + std::to_string(write_buffer[i]);
    if (read_count != 0)
      call += " read " + std::to_string(read_count);
    this->calls.push_back(call);
    return i2c::ERROR_OK;
  }

  i2c::ErrorCode write_prefixed(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *data,
                                size_t len) override {
    std::string call = "prefixed " + std::to_string(address);
    for (size_t i = 0; i < prefix_len; i++)
      call += " " + std::to_string(prefix[i]);
    call += " |";
    for (size_t i = 0; i < len; i++)
      call += " " + std::to_string(data[i]);
    this->calls.push_back(call);
    this->data_ptr = data;
    return i2c::ERROR_OK;
  }
};

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
  EXPECT_EQ(this->bus_.calls, (std::vector<std::string>{"readv 112 8", "prefixed 90 18 52 | 1 2 3", "readv 112 0"}));
  // The caller's buffer is passed through, not a joined copy
  EXPECT_EQ(this->bus_.data_ptr, data);
}

TEST_F(TCA9548AWritePrefixedTest, ReadsAndPlainWritesStillUseWriteReadv) {
  uint8_t out[2];
  EXPECT_EQ(this->device_.read_register(0x10, out, sizeof(out)), i2c::ERROR_OK);
  const uint8_t value = 7;
  EXPECT_EQ(this->device_.write(&value, 1), i2c::ERROR_OK);
  EXPECT_EQ(this->bus_.calls, (std::vector<std::string>{"readv 112 8", "readv 90 16 read 2", "readv 112 0",
                                                        "readv 112 8", "readv 90 7", "readv 112 0"}));
}

}  // namespace esphome::tca9548a::testing
