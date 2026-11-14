#include <gtest/gtest.h>

#include "esphome/components/fan/automation.h"
#include "esphome/components/fan/fan.h"

namespace esphome::fan::testing {

namespace {

// Applies each call at once, as a fan output would
class CyclingFan : public Fan {
 public:
  explicit CyclingFan(int speed_count) : speed_count_(speed_count) {}
  FanTraits get_traits() override { return FanTraits(false, this->speed_count_ > 0, false, this->speed_count_); }

 protected:
  void control(const FanCall &call) override {
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();
    this->publish_state();
  }

  int speed_count_;
};

}  // namespace

TEST(FanCycleSpeedTest, OffFanStartsAtSpeedOne) {
  CyclingFan fan(3);
  cycle_speed(&fan, true);
  EXPECT_TRUE(fan.state);
  EXPECT_EQ(fan.speed, 1);
}

TEST(FanCycleSpeedTest, RunningFanStepsUp) {
  CyclingFan fan(3);
  fan.turn_on().set_speed(2).perform();
  cycle_speed(&fan, true);
  EXPECT_TRUE(fan.state);
  EXPECT_EQ(fan.speed, 3);
}

TEST(FanCycleSpeedTest, TopSpeedTurnsOffWithOffSpeedCycle) {
  CyclingFan fan(3);
  fan.turn_on().set_speed(3).perform();
  cycle_speed(&fan, true);
  EXPECT_FALSE(fan.state);
  EXPECT_EQ(fan.speed, 1);
}

TEST(FanCycleSpeedTest, TopSpeedWrapsToOneWithoutOffSpeedCycle) {
  CyclingFan fan(3);
  fan.turn_on().set_speed(3).perform();
  cycle_speed(&fan, false);
  EXPECT_TRUE(fan.state);
  EXPECT_EQ(fan.speed, 1);
}

TEST(FanCycleSpeedTest, FanWithoutSpeedsToggles) {
  CyclingFan fan(0);
  cycle_speed(&fan, true);
  EXPECT_TRUE(fan.state);
  cycle_speed(&fan, true);
  EXPECT_FALSE(fan.state);
}

}  // namespace esphome::fan::testing
