#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "esphome/core/entity_base.h"

namespace esphome::testing {

struct FakeSensor {
  void publish_state(float state) { this->states.push_back(state); }
  std::vector<float> states;
};

struct FakeBinarySensor {
  void publish_state(bool state) { this->states.push_back(state); }
  std::vector<bool> states;
};

TEST(PublishOptional, PublishesEverySetValue) {
  FakeSensor sensor;
  publish_optional(&sensor, optional<float>(21.5f));
  publish_optional(&sensor, optional<float>(0.0f));
  publish_optional(&sensor, optional<float>(NAN));
  ASSERT_EQ(sensor.states.size(), 3u);
  EXPECT_FLOAT_EQ(sensor.states[0], 21.5f);
  EXPECT_FLOAT_EQ(sensor.states[1], 0.0f);
  EXPECT_TRUE(std::isnan(sensor.states[2]));
}

TEST(PublishOptional, SkipsAnEmptyValue) {
  FakeSensor sensor;
  publish_optional(&sensor, optional<float>());
  EXPECT_TRUE(sensor.states.empty());
}

TEST(PublishOptional, IgnoresAMissingEntity) {
  FakeSensor *sensor = nullptr;
  publish_optional(sensor, optional<float>(21.5f));
  publish_optional(sensor, optional<float>());
}

TEST(PublishOptional, WorksForOtherEntityTypes) {
  FakeBinarySensor binary_sensor;
  publish_optional(&binary_sensor, optional<bool>(false));
  publish_optional(&binary_sensor, optional<bool>());
  publish_optional(&binary_sensor, optional<bool>(true));
  EXPECT_EQ(binary_sensor.states, (std::vector<bool>{false, true}));
}

}  // namespace esphome::testing
