#include <gtest/gtest.h>

#include "esphome/components/hte501/hte501.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::hte501::testing {
namespace {

static constexpr uint8_t ADDRESS = 0x40;

// Answers a measurement read with the raw temperature word (and 50 %RH), each followed by its CRC.
class FakeBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    const uint8_t words[2][2] = {{static_cast<uint8_t>(this->raw >> 8), static_cast<uint8_t>(this->raw)}, {0x13, 0x88}};
    for (size_t i = 0; i + 3 <= out_size; i += 3) {
      out[i] = words[i / 3][0];
      out[i + 1] = words[i / 3][1];
      out[i + 2] = crc8(words[i / 3], 2, 0xFF, 0x31, true);
    }
    return i2c::ERROR_OK;
  }

  uint16_t raw{0};
};

class HTE501TemperatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_i2c_bus(&this->bus_);
    this->sensor_.set_i2c_address(ADDRESS);
    this->sensor_.set_temperature_sensor(&this->temperature_);
  }

  // update() sends the measure command and reads the answer 50 ms later.
  float measure_(uint16_t raw) {
    this->bus_.raw = raw;
    this->sensor_.update();
    delay(60);
    App.scheduler.call(millis());
    return this->temperature_.state;
  }

  FakeBus bus_;
  HTE501Component sensor_;
  sensor::Sensor temperature_;
};

// The sensor sends hundredths of a degree as a signed 16-bit value.
}  // namespace

TEST_F(HTE501TemperatureTest, DecodesSignedHundredths) {
  EXPECT_FLOAT_EQ(this->measure_(0x0960), 24.0f);
  EXPECT_FLOAT_EQ(this->measure_(0x34BC), 135.0f);
  EXPECT_FLOAT_EQ(this->measure_(0xFFFF), -0.01f);
  EXPECT_FLOAT_EQ(this->measure_(0xF060), -40.0f);
}

TEST_F(HTE501TemperatureTest, DecodesTheWholeSignedRange) {
  EXPECT_FLOAT_EQ(this->measure_(0xD8F0), -100.0f);
  EXPECT_FLOAT_EQ(this->measure_(0x8000), -327.68f);
}

}  // namespace esphome::hte501::testing
