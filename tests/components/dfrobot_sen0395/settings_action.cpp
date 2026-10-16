#include <array>
#include <gtest/gtest.h>
#include "esphome/components/dfrobot_sen0395/automation.h"

namespace esphome::dfrobot_sen0395::testing {
namespace {
std::array<int, 8> segment_calls{};
std::array<float, 8> seen_float{};
std::array<int, 8> seen_int{};
int sensitivity_calls;
float sensitivity_argument;

float segment_value(size_t index, float x, int y = 0) {
  segment_calls[index]++;
  seen_float[index] = x;
  seen_int[index] = y;
  return x + y + static_cast<float>(index);
}
}  // namespace

class SettingsActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    segment_calls.fill(0);
    seen_float.fill(0);
    seen_int.fill(0);
    sensitivity_calls = 0;
    sensitivity_argument = 0;
  }

  DfrobotSen0395Component radar_;
};

TEST_F(SettingsActionTest, EmptyTriggerPreservesSegmentCallableCounts) {
  DfrobotSen0395SettingsAction<> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([]() -> float { return segment_value(0, 0); });
  action.set_det_max1([]() -> float { return segment_value(1, 0); });
  action.set_det_min2([]() -> float { return segment_value(2, 0); });
  action.set_det_max2([]() -> float { return segment_value(3, 0); });
  action.set_det_min3([]() -> float { return segment_value(4, 0); });
  action.set_det_max3([]() -> float { return segment_value(5, 0); });
  action.set_det_min4([]() -> float { return segment_value(6, 0); });
  action.set_det_max4([]() -> float { return segment_value(7, 0); });
  action.play();
  for (size_t i = 0; i < segment_calls.size(); i++) {
    EXPECT_EQ(segment_calls[i], i < 2 ? 2 : 1) << i;
  }
}

TEST_F(SettingsActionTest, EmptyTriggerWithoutSegmentsIsValid) {
  DfrobotSen0395SettingsAction<> action;
  action.set_parent(&this->radar_);
  action.play();
  for (auto calls : segment_calls) {
    EXPECT_EQ(calls, 0);
  }
}

TEST_F(SettingsActionTest, EmptyTriggerNegativeMinKeepsShortCircuit) {
  DfrobotSen0395SettingsAction<> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([]() -> float { return segment_value(0, -1); });
  action.set_det_max1([]() -> float { return segment_value(1, 0); });
  action.play();
  EXPECT_EQ(segment_calls[0], 1);
  EXPECT_EQ(segment_calls[1], 0);
}

TEST_F(SettingsActionTest, FloatTriggerForwardsAllSegmentsAndPreservesCounts) {
  DfrobotSen0395SettingsAction<float> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([](float x) -> float { return segment_value(0, x); });
  action.set_det_max1([](float x) -> float { return segment_value(1, x); });
  action.set_det_min2([](float x) -> float { return segment_value(2, x); });
  action.set_det_max2([](float x) -> float { return segment_value(3, x); });
  action.set_det_min3([](float x) -> float { return segment_value(4, x); });
  action.set_det_max3([](float x) -> float { return segment_value(5, x); });
  action.set_det_min4([](float x) -> float { return segment_value(6, x); });
  action.set_det_max4([](float x) -> float { return segment_value(7, x); });
  action.play(2.5f);
  for (size_t i = 0; i < segment_calls.size(); i++) {
    EXPECT_EQ(segment_calls[i], i < 2 ? 2 : 1) << i;
    EXPECT_FLOAT_EQ(seen_float[i], 2.5f) << i;
  }
}

TEST_F(SettingsActionTest, MultipleTriggerArgumentsReachConfiguredSegments) {
  DfrobotSen0395SettingsAction<float, int> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([](float x, int y) -> float { return segment_value(0, x, y); });
  action.set_det_max1([](float x, int y) -> float { return segment_value(1, x, y); });
  action.play(1.5f, 2);
  for (size_t i = 0; i < 2; i++) {
    EXPECT_EQ(segment_calls[i], 2) << i;
    EXPECT_FLOAT_EQ(seen_float[i], 1.5f) << i;
    EXPECT_EQ(seen_int[i], 2) << i;
  }
  for (size_t i = 2; i < segment_calls.size(); i++) {
    EXPECT_EQ(segment_calls[i], 0) << i;
  }
}

TEST_F(SettingsActionTest, FloatTriggerWithoutSegmentsStillEvaluatesSensitivity) {
  DfrobotSen0395SettingsAction<float> action;
  action.set_parent(&this->radar_);
  action.set_sensitivity([](float x) -> int8_t {
    sensitivity_calls++;
    sensitivity_argument = x;
    return 5;
  });
  action.play(3.25f);
  EXPECT_EQ(sensitivity_calls, 1);
  EXPECT_FLOAT_EQ(sensitivity_argument, 3.25f);
  for (auto calls : segment_calls) {
    EXPECT_EQ(calls, 0);
  }
}

TEST_F(SettingsActionTest, FloatTriggerNegativeMinKeepsShortCircuit) {
  DfrobotSen0395SettingsAction<float> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([](float x) -> float { return segment_value(0, x); });
  action.set_det_max1([](float x) -> float { return segment_value(1, x); });
  action.play(-1.0f);
  EXPECT_EQ(segment_calls[0], 1);
  EXPECT_EQ(segment_calls[1], 0);
  EXPECT_FLOAT_EQ(seen_float[0], -1.0f);
}

TEST_F(SettingsActionTest, FloatTriggerUnpairedSegmentIsNotEvaluated) {
  DfrobotSen0395SettingsAction<float> action;
  action.set_parent(&this->radar_);
  action.set_det_min1([](float x) -> float { return segment_value(0, x); });
  action.play(4.0f);
  EXPECT_EQ(segment_calls[0], 0);
}

}  // namespace esphome::dfrobot_sen0395::testing
