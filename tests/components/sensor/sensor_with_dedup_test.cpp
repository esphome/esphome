#include <gtest/gtest.h>
#include <cmath>

#include "esphome/components/sensor/sensor_with_dedup.h"

namespace esphome::sensor::testing {

class SensorWithDedupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](float value) { this->published_.push_back(value); });
  }

  Sensor sensor_;
  std::vector<float> published_;
};

TEST_F(SensorWithDedupTest, NoSensorDoesNothing) {
  SensorWithDedup<uint8_t> dedup;
  EXPECT_FALSE(dedup.has_sensor());
  EXPECT_EQ(dedup.get_sensor(), nullptr);
  dedup.publish_state_if_not_dup(5);
  dedup.publish_state_unknown();
  EXPECT_TRUE(this->published_.empty());
}

TEST_F(SensorWithDedupTest, SkipsRepeatedValues) {
  SensorWithDedup<int16_t> dedup;
  dedup.set_sensor(&this->sensor_);
  EXPECT_TRUE(dedup.has_sensor());
  EXPECT_EQ(dedup.get_sensor(), &this->sensor_);
  dedup.publish_state_if_not_dup(-3);
  dedup.publish_state_if_not_dup(-3);
  dedup.publish_state_if_not_dup(7);
  dedup.publish_state_if_not_dup(7);
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_EQ(this->published_[0], -3.0f);
  EXPECT_EQ(this->published_[1], 7.0f);
}

TEST_F(SensorWithDedupTest, UnknownPublishedOnceThenValueAgain) {
  SensorWithDedup<float> dedup;
  dedup.set_sensor(&this->sensor_);
  dedup.publish_state_if_not_dup(1.5f);
  dedup.publish_state_unknown();
  dedup.publish_state_unknown();
  dedup.publish_state_if_not_dup(1.5f);
  ASSERT_EQ(this->published_.size(), 3u);
  EXPECT_EQ(this->published_[0], 1.5f);
  EXPECT_TRUE(std::isnan(this->published_[1]));
  EXPECT_EQ(this->published_[2], 1.5f);
}

TEST_F(SensorWithDedupTest, SetSensorResetsHistory) {
  SensorWithDedup<uint16_t> dedup;
  dedup.set_sensor(&this->sensor_);
  dedup.publish_state_if_not_dup(100);
  dedup.set_sensor(&this->sensor_);
  dedup.publish_state_if_not_dup(100);
  EXPECT_EQ(this->published_.size(), 2u);
}

}  // namespace esphome::sensor::testing
