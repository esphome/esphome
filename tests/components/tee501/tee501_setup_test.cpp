#include <gtest/gtest.h>

#include "esphome/components/tee501/tee501.h"
#include "esphome/core/helpers.h"

namespace esphome::tee501::testing {
namespace {

constexpr uint8_t ADDRESS = 0x48;

// Answers the read identification command (0x7029) and NACKs a read sent without a command.
class FakeBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    this->transactions++;
    if (size != 2 || data[0] != 0x70 || data[1] != 0x29 || out_size != 9)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    static constexpr uint8_t SERIAL[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    for (size_t i = 0; i < 8; i++)
      out[i] = SERIAL[i];
    out[8] = crc8(SERIAL, 8, 0xFF, 0x31, true);
    return i2c::ERROR_OK;
  }

  unsigned transactions{0};
};

}  // namespace

TEST(TEE501SetupTest, ReadsIdentificationInOneTransaction) {
  FakeBus bus;
  TEE501Component tee501;
  tee501.set_i2c_bus(&bus);
  tee501.set_i2c_address(ADDRESS);

  tee501.setup();

  EXPECT_EQ(bus.transactions, 1u);
  EXPECT_FALSE(tee501.is_failed());
}

}  // namespace esphome::tee501::testing
