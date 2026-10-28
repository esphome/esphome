#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

class PublishStateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](float value) { this->published_.push_back(value); });
  }

  float next_value_() {
    this->evaluations_++;
    return 4.5f;
  }

  Sensor sensor_;
  std::vector<float> published_;
  int evaluations_{0};
};

TEST_F(PublishStateTest, PublishesWhenSet) {
  Sensor *sensor = &this->sensor_;
  PUBLISH_STATE(sensor, this->next_value_());
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_EQ(this->published_[0], 4.5f);
  EXPECT_EQ(this->evaluations_, 1);
}

TEST_F(PublishStateTest, NullDoesNotEvaluateValue) {
  Sensor *sensor = nullptr;
  PUBLISH_STATE(sensor, this->next_value_());
  EXPECT_TRUE(this->published_.empty());
  EXPECT_EQ(this->evaluations_, 0);
}

TEST_F(PublishStateTest, ActsAsOneStatement) {
  Sensor *sensor = &this->sensor_;
  bool skip = true;
  if (skip)
    PUBLISH_STATE(sensor, 1.0f);
  else
    PUBLISH_STATE(sensor, 2.0f);
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_EQ(this->published_[0], 1.0f);
}

// Any entity with a one-argument publish_state() works, not only sensors.
struct FakeSwitch {
  void publish_state(bool state) { this->states.push_back(state); }
  std::vector<bool> states;
};

TEST(PublishStateOtherEntity, PublishesOnAnyEntityType) {
  FakeSwitch sw;
  FakeSwitch *set = &sw;
  FakeSwitch *unset = nullptr;
  PUBLISH_STATE(set, true);
  PUBLISH_STATE(unset, false);
  ASSERT_EQ(sw.states.size(), 1u);
  EXPECT_TRUE(sw.states[0]);
}

}  // namespace esphome::sensor::testing
