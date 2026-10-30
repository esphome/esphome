#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "../common.h"
#include "esphome/components/ufire_ec/ufire_ec.h"

namespace esphome::ufire_ec::testing {

class ExternalTemperatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ec_.set_i2c_bus(&this->bus_);
    this->ec_.set_i2c_address(0x3C);
    this->ec_.set_external_temperature_sensor(&this->external_);
  }

  RecordingBus bus_;
  sensor::Sensor external_;
  UFireECComponent ec_;
};

TEST_F(ExternalTemperatureTest, NoReadingYetWritesNothing) {
  this->ec_.update();
  EXPECT_EQ(this->bus_.count(REGISTER_TEMP), 0u);
}

TEST_F(ExternalTemperatureTest, NanReadingWritesNothing) {
  this->external_.publish_state(NAN);
  this->ec_.update();
  EXPECT_EQ(this->bus_.count(REGISTER_TEMP), 0u);
}

TEST_F(ExternalTemperatureTest, ReadingIsWritten) {
  this->external_.publish_state(21.5f);
  this->ec_.update();
  ASSERT_EQ(this->bus_.count(REGISTER_TEMP), 1u);
  const auto &w = this->bus_.writes.back();
  ASSERT_EQ(w.size(), 5u);
  float written;
  std::memcpy(&written, w.data() + 1, sizeof(written));
  EXPECT_EQ(written, 21.5f);
}

}  // namespace esphome::ufire_ec::testing
