#include <gtest/gtest.h>

#include <vector>
#include "esphome/components/i2c/i2c.h"

namespace esphome::i2c::testing {

// A bus that only implements write_readv(), like external buses, so write_prefixed() takes the default path
class JoiningBus : public I2CBus {
 public:
  std::vector<uint8_t> written;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    this->written.assign(write_buffer, write_buffer + write_count);
    return ERROR_OK;
  }
};

// A bus that takes the two parts separately, like the built-in buses
class GatherBus final : public JoiningBus {
 public:
  std::vector<uint8_t> prefix;
  std::vector<uint8_t> data;
  const uint8_t *data_ptr{nullptr};
  ErrorCode write_prefixed(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *data,
                           size_t len) override {
    this->prefix.assign(prefix, prefix + prefix_len);
    this->data.assign(data, data + len);
    this->data_ptr = data;
    return ERROR_OK;
  }
};

static std::vector<uint8_t> pattern(size_t len) {
  std::vector<uint8_t> bytes(len);
  for (size_t i = 0; i < len; i++)
    bytes[i] = static_cast<uint8_t>(i * 7 + 1);
  return bytes;
}

TEST(I2CWritePrefixed, DefaultJoinsPrefixAndDataForShortAndLongWrites) {
  // 18 data bytes fit the stack buffer with a 2-byte prefix; 300 need the heap fallback
  for (size_t len : {0, 1, 18, 19, 300}) {
    JoiningBus bus;
    const uint8_t prefix[2] = {0xAB, 0xCD};
    const std::vector<uint8_t> data = pattern(len);
    EXPECT_EQ(bus.write_prefixed(0x10, prefix, sizeof(prefix), data.data(), data.size()), ERROR_OK);
    std::vector<uint8_t> expected{0xAB, 0xCD};
    expected.insert(expected.end(), data.begin(), data.end());
    EXPECT_EQ(bus.written, expected) << "length " << len;
  }
}

TEST(I2CWritePrefixed, WriteRegisterPassesTheRegisterAsPrefixWithoutCopyingTheData) {
  GatherBus bus;
  I2CDevice device;
  device.set_i2c_bus(&bus);
  device.set_i2c_address(0x10);
  const std::vector<uint8_t> data = pattern(64);

  EXPECT_EQ(device.write_register(0x5E, data.data(), data.size()), ERROR_OK);
  EXPECT_EQ(bus.prefix, std::vector<uint8_t>{0x5E});
  EXPECT_EQ(bus.data, data);
  EXPECT_EQ(bus.data_ptr, data.data());

  EXPECT_EQ(device.write_register16(0x1234, data.data(), data.size()), ERROR_OK);
  EXPECT_EQ(bus.prefix, (std::vector<uint8_t>{0x12, 0x34}));
  EXPECT_EQ(bus.data_ptr, data.data());
}

}  // namespace esphome::i2c::testing
