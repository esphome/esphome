#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/teleinfo/sensor/teleinfo_sensor.h"

namespace esphome::teleinfo::testing {

namespace {
class TeleInfoSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](float state) {
      this->publishes_++;
      this->last_ = state;
    });
  }

  TeleInfoSensor sensor_{"EAST"};
  int publishes_{0};
  float last_{NAN};
};
}  // namespace

TEST_F(TeleInfoSensorTest, PublishesZeroPaddedNumber) {
  this->sensor_.publish_val("000012345");
  EXPECT_EQ(this->publishes_, 1);
  EXPECT_FLOAT_EQ(this->last_, 12345.0f);
}

TEST_F(TeleInfoSensorTest, PublishesRealZero) {
  this->sensor_.publish_val("000000000");
  EXPECT_EQ(this->publishes_, 1);
  EXPECT_FLOAT_EQ(this->last_, 0.0f);
}

// A value that is not a number must be dropped: publishing 0 would look like a meter reset
TEST_F(TeleInfoSensorTest, SkipsValueThatIsNotANumber) {
  this->sensor_.publish_val("000012345");
  this->sensor_.publish_val("0000l2345");
  this->sensor_.publish_val("H081225223518");
  EXPECT_EQ(this->publishes_, 1);
  EXPECT_FLOAT_EQ(this->last_, 12345.0f);
  EXPECT_FLOAT_EQ(this->sensor_.get_state(), 12345.0f);
}

}  // namespace esphome::teleinfo::testing
