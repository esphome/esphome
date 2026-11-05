#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/tee501/tee501.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::tee501::testing {
namespace {

constexpr uint8_t ADDRESS = 0x40;

// Always fills the read buffer with a valid answer, so a transfer reported as failed still carries a
// frame that passes the CRC check.
class FakeBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    // 24.00 °C, 50.00 %RH
    static constexpr uint8_t MEASUREMENT[2][2] = {{0x09, 0x60}, {0x13, 0x88}};
    for (size_t i = 0; i + 3 <= out_size; i += 3) {
      const uint8_t *word = MEASUREMENT[i / 3 % 2];
      out[i] = word[0];
      out[i + 1] = word[1];
      out[i + 2] = crc8(word, 2, 0xFF, 0x31, true);
    }
    if (out_size == 9)  // identification: CRC over all 8 bytes
      out[8] = crc8(out, 8, 0xFF, 0x31, true);
    if (size != 0)
      return this->fail_writes ? i2c::ERROR_NOT_ACKNOWLEDGED : i2c::ERROR_OK;
    return this->fail_reads ? i2c::ERROR_TIMEOUT : i2c::ERROR_OK;
  }

  bool fail_writes{false};
  bool fail_reads{false};
};

class TEE501I2CErrorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_i2c_bus(&this->bus_);
    this->sensor_.set_i2c_address(ADDRESS);
  }

  // update() sends the measure command and reads the answer 50 ms later.
  void measure_() {
    this->temperature_.state = NAN;
    this->sensor_.update();
    delay(60);  // NOLINT
    App.scheduler.call(millis());
  }

  FakeBus bus_;
  TEE501Component sensor_;
  sensor::Sensor &temperature_{this->sensor_};
};

}  // namespace

TEST_F(TEE501I2CErrorTest, MeasuresWhenTheBusWorks) {
  this->measure_();
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.0f);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(TEE501I2CErrorTest, SetupFailsWhenIdentificationReadFails) {
  this->bus_.fail_writes = true;
  this->sensor_.setup();
  EXPECT_TRUE(this->sensor_.is_failed());
}

TEST_F(TEE501I2CErrorTest, FailedCommandSkipsTheReading) {
  this->bus_.fail_writes = true;
  this->measure_();
  EXPECT_TRUE(std::isnan(this->temperature_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(TEE501I2CErrorTest, FailedReadSkipsTheReading) {
  this->bus_.fail_reads = true;
  this->measure_();
  EXPECT_TRUE(std::isnan(this->temperature_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

}  // namespace esphome::tee501::testing
