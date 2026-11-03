#include <gtest/gtest.h>

#include "esphome/components/tmp1075/tmp1075.h"

namespace esphome::tmp1075::testing {

// Upper byte of CFGR per the TMP1075 datasheet: OS R1 R0 F1 F0 POL TM SD

TEST(TMP1075Config, DefaultsAreZero) { EXPECT_EQ(TMP1075Config{}.to_byte(), 0x00); }

TEST(TMP1075Config, EachFieldLandsOnItsDatasheetBit) {
  TMP1075Config cfg{};
  cfg.shutdown = true;
  EXPECT_EQ(cfg.to_byte(), 0x01);

  cfg = {};
  cfg.alert_mode = true;
  EXPECT_EQ(cfg.to_byte(), 0x02);

  cfg = {};
  cfg.polarity = true;
  EXPECT_EQ(cfg.to_byte(), 0x04);

  cfg = {};
  cfg.faults = 3;
  EXPECT_EQ(cfg.to_byte(), 0x18);

  cfg = {};
  cfg.rate = 3;
  EXPECT_EQ(cfg.to_byte(), 0x60);

  cfg = {};
  cfg.oneshot = true;
  EXPECT_EQ(cfg.to_byte(), 0x80);
}

TEST(TMP1075Config, ActiveHighComparatorAt220ms) {
  // The configuration from issue 21381: 220 ms, 1 fault, active high, comparator
  TMP1075Config cfg{};
  cfg.rate = 3;
  cfg.polarity = true;
  EXPECT_EQ(cfg.to_byte(), 0x64);
}

// Records what the driver writes to the configuration register
class CaptureBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    if (read_count != 0) {
      for (size_t i = 0; i < read_count; i++) {
        read_buffer[i] = 0;
      }
      return i2c::ERROR_OK;
    }
    if (write_count == 2 && write_buffer[0] == 0x01) {
      this->cfgr = write_buffer[1];
      this->cfgr_writes++;
    }
    return i2c::ERROR_OK;
  }

  uint8_t cfgr{0xFF};
  int cfgr_writes{0};
};

TEST(TMP1075Sensor, WriteConfigSendsDatasheetLayout) {
  CaptureBus bus;
  TMP1075Sensor sensor;
  sensor.set_i2c_bus(&bus);
  sensor.set_i2c_address(0x48);

  sensor.set_conversion_rate(CONV_RATE_220_MS);
  sensor.set_fault_count(1);
  sensor.set_alert_polarity(true);
  sensor.set_alert_function(ALERT_COMPARATOR);
  sensor.write_config();

  EXPECT_EQ(bus.cfgr_writes, 1);
  EXPECT_EQ(bus.cfgr, 0x64);
}

}  // namespace esphome::tmp1075::testing
