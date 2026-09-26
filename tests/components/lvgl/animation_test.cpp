#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "esphome/components/lvgl/animation.h"

namespace esphome::lvgl::testing {

namespace {

// The update callback is a plain function pointer, so it cannot capture; results go to a global.
std::vector<lv_coord_t> updates;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void record_update(const lv_coord_t *data) { updates.push_back(data[0]); }

// Exposes the running state so tests can wait for the animation to finish.
class TestAnimation : public LvAnimation<1> {
 public:
  TestAnimation(lv_coord_t from, lv_coord_t to, uint32_t duration_ms, uint32_t start_delay_ms = 0)
      : LvAnimation<1>(record_update, {TemplatableValue<lv_coord_t>(from)}, {TemplatableValue<lv_coord_t>(to)}) {
    this->set_duration(duration_ms);
    this->set_start_delay(start_delay_ms);
    this->add_on_start_callback([this]() { this->start_count++; });
    this->add_on_stop_callback([this]() { this->stop_count++; });
  }

  bool is_running() const { return this->state_ != AnimationState::STOPPED; }

  // Returns true if the animation stopped before the timeout.
  bool run_until_stopped(uint32_t timeout_ms = 1000) {
    const uint32_t begin = millis();
    while (this->is_running() && millis() - begin < timeout_ms) {
      this->loop();
      delay(1);
    }
    return !this->is_running();
  }

  void run_for(uint32_t duration_ms) {
    const uint32_t begin = millis();
    while (millis() - begin < duration_ms) {
      this->loop();
      delay(1);
    }
  }

  int start_count{0};
  int stop_count{0};
};

lv_coord_t max_update() {
  lv_coord_t result = updates.front();
  for (auto value : updates)
    result = std::max(result, value);
  return result;
}

}  // namespace

class LvAnimationTest : public ::testing::Test {
 protected:
  void SetUp() override { updates.clear(); }
};

TEST_F(LvAnimationTest, LinearRunsToEndAndStopsOnce) {
  TestAnimation anim(0, 100, 20);
  anim.start();
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(updates.front(), 0);
  EXPECT_EQ(updates.back(), 100);
  EXPECT_EQ(anim.start_count, 1);
  EXPECT_EQ(anim.stop_count, 1);

  // Nothing more happens once stopped.
  const size_t update_count = updates.size();
  anim.run_for(10);
  EXPECT_EQ(updates.size(), update_count);
  EXPECT_EQ(anim.stop_count, 1);
}

TEST_F(LvAnimationTest, ZeroDurationDoesNotStart) {
  TestAnimation anim(0, 100, 0);
  anim.start();
  EXPECT_FALSE(anim.is_running());
  EXPECT_EQ(anim.start_count, 0);
  EXPECT_TRUE(updates.empty());
}

TEST_F(LvAnimationTest, StartDelayHoldsBackUpdates) {
  TestAnimation anim(0, 100, 20, 30);
  anim.start();
  EXPECT_TRUE(anim.is_running());
  EXPECT_TRUE(updates.empty());
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(updates.back(), 100);
}

// Round trip maps the end of the duration back to the start value, so completion must not depend on the
// mapped value reaching 1.0.
TEST_F(LvAnimationTest, RoundTripStopsAtStartValue) {
  LvAnimationTimingRoundTrip timing(0.0f);
  TestAnimation anim(0, 100, 20);
  anim.add_timing(&timing);
  anim.start();
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(updates.back(), 0);
  EXPECT_EQ(anim.stop_count, 1);
}

// The pause maps to 1.0 in the middle of the duration; the animation must still play the return leg.
TEST_F(LvAnimationTest, RoundTripWithPausePlaysReturnLeg) {
  LvAnimationTimingRoundTrip timing(0.5f);
  TestAnimation anim(0, 100, 40);
  anim.add_timing(&timing);
  anim.start();
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(max_update(), 100);
  EXPECT_EQ(updates.back(), 0);
  EXPECT_EQ(anim.stop_count, 1);
}

TEST_F(LvAnimationTest, GravityStops) {
  LvAnimationTimingGravity timing(0.5f, 0.5f);
  TestAnimation anim(0, 100, 20);
  anim.add_timing(&timing);
  anim.start();
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(anim.stop_count, 1);
}

TEST_F(LvAnimationTest, EaseInOutEndsAtTarget) {
  LvAnimationTimingEaseInOut timing(1.0f);
  TestAnimation anim(0, 100, 20);
  anim.add_timing(&timing);
  anim.start();
  ASSERT_TRUE(anim.run_until_stopped());
  EXPECT_EQ(updates.back(), 100);
}

TEST_F(LvAnimationTest, LoopRestartsOnlyAfterReachingEnd) {
  TestAnimation anim(0, 100, 20);
  anim.set_loop(true);
  anim.start();
  anim.run_for(100);
  anim.stop();

  EXPECT_GE(anim.start_count, 2);
  // Each restart is a drop in value, and must follow a completed cycle that reached the end value.
  for (size_t i = 1; i < updates.size(); i++) {
    if (updates[i] < updates[i - 1])
      EXPECT_EQ(updates[i - 1], 100) << "update " << i << " restarted before the end was reached";
  }
}

// The first frame of the next cycle must not be drawn in the same pass as the final frame of the last one.
TEST_F(LvAnimationTest, LoopRestartKeepsFinalFrame) {
  TestAnimation anim(0, 100, 20);
  anim.set_loop(true);
  anim.start();
  const uint32_t begin = millis();
  while (millis() - begin < 100) {
    const size_t before = updates.size();
    anim.loop();
    ASSERT_LE(updates.size() - before, 1u);
    delay(1);
  }
  anim.stop();
  EXPECT_GE(anim.start_count, 2);
}

TEST_F(LvAnimationTest, LoopWithRoundTripKeepsCycling) {
  LvAnimationTimingRoundTrip timing(0.0f);
  TestAnimation anim(0, 100, 20);
  anim.add_timing(&timing);
  anim.set_loop(true);
  anim.start();
  anim.run_for(100);
  EXPECT_GE(anim.start_count, 2);
  EXPECT_GE(anim.stop_count, 1);
  anim.stop();
}

}  // namespace esphome::lvgl::testing
