#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

class PublishOptionalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](float value) { this->published_.push_back(value); });
  }

  Sensor sensor_;
  std::vector<float> published_;
};

TEST_F(PublishOptionalTest, PublishesEverySetValue) {
  publish_optional(&this->sensor_, 21.5f);
  publish_optional(&this->sensor_, 0.0f);
  publish_optional(&this->sensor_, NAN);
  ASSERT_EQ(this->published_.size(), 3u);
  EXPECT_FLOAT_EQ(this->published_[0], 21.5f);
  EXPECT_FLOAT_EQ(this->published_[1], 0.0f);
  EXPECT_TRUE(std::isnan(this->published_[2]));
}

TEST_F(PublishOptionalTest, SkipsAnEmptyValue) {
  publish_optional(&this->sensor_, nullopt);
  EXPECT_TRUE(this->published_.empty());
  EXPECT_FALSE(this->sensor_.has_state());
}

TEST_F(PublishOptionalTest, IgnoresAMissingSensor) {
  publish_optional(nullptr, 21.5f);
  publish_optional(nullptr, nullopt);
}

}  // namespace esphome::sensor::testing
