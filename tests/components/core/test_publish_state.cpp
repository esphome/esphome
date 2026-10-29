#include <gtest/gtest.h>
#include <cstddef>
#include <string>
#include <vector>

#include "esphome/core/entity_base.h"

namespace esphome::testing {

struct FakeSensor {
  void publish_state(float state) { this->states.push_back(state); }
  std::vector<float> states;
};

struct FakeTextSensor {
  void publish_state(const char *state, size_t len) { this->states.emplace_back(state, len); }
  std::vector<std::string> states;
};

TEST(PublishState, PublishesWhenSet) {
  FakeSensor sensor;
  FakeSensor *ptr = &sensor;
  PUBLISH_STATE(ptr, 4.5f);
  ASSERT_EQ(sensor.states.size(), 1u);
  EXPECT_EQ(sensor.states[0], 4.5f);
}

TEST(PublishState, NullDoesNotEvaluateValue) {
  FakeSensor *ptr = nullptr;
  int evaluations = 0;
  PUBLISH_STATE(ptr, (evaluations++, 1.0f));
  EXPECT_EQ(evaluations, 0);
}

TEST(PublishState, EntityEvaluatedOnce) {
  FakeSensor sensor;
  int lookups = 0;
  auto get = [&]() {
    lookups++;
    return &sensor;
  };
  PUBLISH_STATE(get(), 2.0f);
  EXPECT_EQ(lookups, 1);
  EXPECT_EQ(sensor.states.size(), 1u);
}

TEST(PublishState, ActsAsOneStatement) {
  FakeSensor sensor;
  FakeSensor *ptr = &sensor;
  bool first = true;
  // NOLINTBEGIN(readability-braces-around-statements)
  if (first)
    PUBLISH_STATE(ptr, 1.0f);
  else
    PUBLISH_STATE(ptr, 2.0f);
  // NOLINTEND(readability-braces-around-statements)
  ASSERT_EQ(sensor.states.size(), 1u);
  EXPECT_EQ(sensor.states[0], 1.0f);
}

TEST(PublishState, PassesSeveralArguments) {
  FakeTextSensor text;
  FakeTextSensor *ptr = &text;
  FakeTextSensor *unset = nullptr;
  PUBLISH_STATE(ptr, "abcdef", 3);
  PUBLISH_STATE(unset, "abcdef", 3);
  ASSERT_EQ(text.states.size(), 1u);
  EXPECT_EQ(text.states[0], "abc");
}

// Caller names must not collide with the macro's temporary.
TEST(PublishState, CallerEntityNamedLikeTemporary) {
  FakeSensor sensor;
  FakeSensor *publish_state_target = &sensor;
  PUBLISH_STATE(publish_state_target, 1.0f);
  ASSERT_EQ(sensor.states.size(), 1u);
  EXPECT_EQ(sensor.states[0], 1.0f);
}

TEST(PublishState, CallerArgumentNamedLikeTemporary) {
  FakeSensor sensor;
  FakeSensor *ptr = &sensor;
  float publish_state_target = 2.0f;
  PUBLISH_STATE(ptr, publish_state_target);
  ASSERT_EQ(sensor.states.size(), 1u);
  EXPECT_EQ(sensor.states[0], 2.0f);
}

}  // namespace esphome::testing
