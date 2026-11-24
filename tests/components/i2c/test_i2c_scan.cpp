#include <gtest/gtest.h>

#include <set>
#include <span>
#include <vector>

#include "esphome/components/i2c/i2c.h"

namespace esphome::i2c::testing {

class ScanBus : public I2CBus {
 public:
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override {
    this->probed.push_back(address);
    if (this->errors.contains(address))
      return ERROR_UNKNOWN;
    return this->present.contains(address) ? ERROR_OK : ERROR_NOT_ACKNOWLEDGED;
  }

  void scan() { this->i2c_scan_(); }
  std::vector<uint8_t> results() const {
    std::span<const uint8_t> span = this->scan_results_;
    return {span.begin(), span.end()};
  }

  std::set<uint8_t> present;
  std::set<uint8_t> errors;
  std::vector<uint8_t> probed;
};

TEST(I2CScan, ProbesTheUnreservedAddresses) {
  ScanBus bus;
  bus.scan();
  ASSERT_EQ(bus.probed.size(), 112u);
  EXPECT_EQ(bus.probed.front(), 0x08);
  EXPECT_EQ(bus.probed.back(), 0x77);
  EXPECT_TRUE(bus.results().empty());
}

TEST(I2CScan, KeepsFoundAddressesInOrder) {
  ScanBus bus;
  bus.present = {0x48, 0x10, 0x76, 0x3C};
  bus.scan();
  EXPECT_EQ(bus.results(), (std::vector<uint8_t>{0x10, 0x3C, 0x48, 0x76}));
}

TEST(I2CScan, MarksUnknownErrorsInBitSeven) {
  ScanBus bus;
  bus.present = {0x20, 0x77};
  bus.errors = {0x08, 0x40};
  bus.scan();
  EXPECT_EQ(bus.results(), (std::vector<uint8_t>{0x88, 0x20, 0xC0, 0x77}));
}

TEST(I2CScan, StuckBusKeepsAllAddresses) {
  ScanBus bus;
  for (uint8_t address = 0x08; address < 0x78; address++)
    bus.present.insert(address);
  bus.scan();
  ASSERT_EQ(bus.probed.size(), 112u);
  EXPECT_EQ(bus.results(), bus.probed);
}

TEST(I2CScan, RescanReplacesTheResults) {
  ScanBus bus;
  bus.present = {0x10, 0x20, 0x30, 0x40, 0x50};
  bus.scan();
  EXPECT_EQ(bus.results(), (std::vector<uint8_t>{0x10, 0x20, 0x30, 0x40, 0x50}));
  bus.present = {0x60};
  bus.scan();
  EXPECT_EQ(bus.results(), (std::vector<uint8_t>{0x60}));
}

}  // namespace esphome::i2c::testing
