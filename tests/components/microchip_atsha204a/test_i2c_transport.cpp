#include <gtest/gtest.h>

#include "esphome/components/microchip_atsha204a/transport.h"

namespace esphome::microchip_atsha204a::testing {

class TestI2CBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t, const uint8_t *, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(write_count, 0U);
    if (read_count == 1) {
      if (this->first_read == i2c::ERROR_OK)
        read_buffer[0] = 4;
      return this->first_read;
    }
    EXPECT_EQ(read_count, 3U);
    ++this->body_reads;
    return this->body_read;
  }

  i2c::ErrorCode first_read{i2c::ERROR_OK};
  i2c::ErrorCode body_read{i2c::ERROR_OK};
  unsigned body_reads{0};
};

TEST(ATSHA204AI2C, OnlyRetriesNackOrTimeoutBeforeReadingThePacket) {
  TestI2CBus bus;
  I2CTransport transport(25000);
  transport.set_i2c_bus(&bus);
  uint8_t packet[MAX_RESPONSE]{};

  bus.first_read = i2c::ERROR_NOT_ACKNOWLEDGED;
  EXPECT_EQ(transport.receive(packet), Receive::RECEIVE_PENDING);
  bus.first_read = i2c::ERROR_TIMEOUT;
  EXPECT_EQ(transport.receive(packet), Receive::RECEIVE_PENDING);
  bus.first_read = i2c::ERROR_NOT_INITIALIZED;
  EXPECT_EQ(transport.receive(packet), Receive::RECEIVE_IO_ERROR);
  EXPECT_EQ(bus.body_reads, 0U);

  bus.first_read = i2c::ERROR_OK;
  EXPECT_EQ(transport.receive(packet), Receive::RECEIVE_COMPLETE);
  bus.body_read = i2c::ERROR_TIMEOUT;
  EXPECT_EQ(transport.receive(packet), Receive::RECEIVE_IO_ERROR);
  EXPECT_EQ(bus.body_reads, 2U);
}

}  // namespace esphome::microchip_atsha204a::testing
