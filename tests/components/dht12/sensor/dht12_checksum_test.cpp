#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/dht12/dht12.h"
#include "esphome/components/sensor/sensor.h"
#include "../common.h"

namespace esphome::dht12::testing {

namespace {

class DHT12ChecksumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_i2c_bus(&this->bus_);
    this->sut_.set_i2c_address(ADDRESS);
    this->sut_.set_temperature_sensor(&this->temperature_);
    this->sut_.set_humidity_sensor(&this->humidity_);
  }

  FrameBus bus_;
  DHT12Component sut_;
  sensor::Sensor temperature_;
  sensor::Sensor humidity_;
};

// The fifth byte is the low byte of the sum of the first four.
TEST_F(DHT12ChecksumTest, ValidFramePublishes) {
  this->bus_.frame = {55, 2, 24, 5, 0x56};
  this->sut_.update();
  EXPECT_FLOAT_EQ(this->humidity_.state, 55.2f);
  EXPECT_FLOAT_EQ(this->temperature_.state, 24.5f);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(DHT12ChecksumTest, ChecksumDropsCarry) {
  // 0x99 + 0x09 + 0x7F + 0x89 = 0x1AA; bit 7 of byte 3 makes the temperature negative.
  this->bus_.frame = {0x99, 0x09, 0x7F, 0x89, 0xAA};
  this->sut_.update();
  EXPECT_FLOAT_EQ(this->humidity_.state, 153.9f);
  EXPECT_FLOAT_EQ(this->temperature_.state, -127.9f);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(DHT12ChecksumTest, BadChecksumWarns) {
  this->bus_.frame = {55, 2, 24, 5, 0x57};
  this->sut_.update();
  EXPECT_TRUE(std::isnan(this->humidity_.state));
  EXPECT_TRUE(std::isnan(this->temperature_.state));
  EXPECT_TRUE(this->sut_.status_has_warning());
}

}  // namespace

}  // namespace esphome::dht12::testing
