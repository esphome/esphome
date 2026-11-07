#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "esphome/components/gy39/gy39.h"

namespace esphome::gy39::testing {

static constexpr uint8_t ADDRESS = 0x5B;

// Registers 0x00-0x0D read in one transfer.
class FrameBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    EXPECT_EQ(size, 1u);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(out_size, this->frame.size());
    std::copy(this->frame.begin(), this->frame.end(), out);
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 14> frame{};
  bool fail{false};
};

class GY39Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_i2c_address(ADDRESS);
    this->chip_.set_illuminance_sensor(&this->illuminance_);
    this->chip_.set_temperature_sensor(&this->temperature_);
    this->chip_.set_pressure_sensor(&this->pressure_);
    this->chip_.set_humidity_sensor(&this->humidity_);
    this->chip_.set_altitude_sensor(&this->altitude_);
  }

  FrameBus bus_;
  GY39 chip_;
  sensor::Sensor illuminance_;
  sensor::Sensor temperature_;
  sensor::Sensor pressure_;
  sensor::Sensor humidity_;
  sensor::Sensor altitude_;
};

TEST_F(GY39Test, DecodesAllFiveValues) {
  // 1234.56 lx, 21.50 °C, 101325.00 Pa, 45.67 %, 123 m
  this->bus_.frame = {0x00, 0x01, 0xE2, 0x40, 0x08, 0x66, 0x00, 0x9A, 0x9C, 0x14, 0x11, 0xD7, 0x00, 0x7B};
  this->chip_.update();
  EXPECT_FLOAT_EQ(this->illuminance_.state, 1234.56f);
  EXPECT_FLOAT_EQ(this->temperature_.state, 21.5f);
  EXPECT_FLOAT_EQ(this->pressure_.state, 101325.0f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 45.67f);
  EXPECT_FLOAT_EQ(this->altitude_.state, 123.0f);
}

TEST_F(GY39Test, TemperatureAndAltitudeAreSigned) {
  // -5.00 °C, -12 m
  this->bus_.frame[4] = 0xFE;
  this->bus_.frame[5] = 0x0C;
  this->bus_.frame[12] = 0xFF;
  this->bus_.frame[13] = 0xF4;
  this->chip_.update();
  EXPECT_FLOAT_EQ(this->temperature_.state, -5.0f);
  EXPECT_FLOAT_EQ(this->altitude_.state, -12.0f);
}

TEST_F(GY39Test, FailedReadWarnsWithoutPublishingUntilTheNextGoodRead) {
  this->bus_.fail = true;
  this->chip_.update();
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_FALSE(this->temperature_.has_state());

  this->bus_.fail = false;
  this->bus_.frame[5] = 100;  // 1.00 °C
  this->chip_.update();
  EXPECT_FALSE(this->chip_.status_has_warning());
  EXPECT_FLOAT_EQ(this->temperature_.state, 1.0f);
}

}  // namespace esphome::gy39::testing
