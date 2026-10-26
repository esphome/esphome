#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/text_sensor/filter.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/application.h"

namespace esphome::text_sensor::testing {

class TextFilterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](const std::string &state) { this->seen_.push_back(state); });
  }
  void publish_at(uint32_t now, const char *value) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    this->sensor_.publish_state(value);
  }
  static void loop_at(Component &filter, uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    filter.loop();
  }

  TextSensor sensor_;
  std::vector<std::string> seen_;
};

static constexpr const char *PRIORITIZED[] = {"NA", nullptr};

TEST_F(TextFilterTest, ThrottleDropsValuesInsideThePeriod) {
  ThrottleFilter throttle(1000);
  this->sensor_.add_filter(&throttle);
  this->publish_at(1000, "a");
  this->publish_at(1500, "b");
  this->publish_at(2000, "c");
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "c"}));
}

TEST_F(TextFilterTest, PrioritizedValuePassesAtOnce) {
  ThrottleWithPriorityFilter throttle(1000, PRIORITIZED);
  this->sensor_.add_filter(&throttle);
  this->publish_at(1000, "a");
  this->publish_at(1100, "NA");
  this->publish_at(1200, "b");
  this->publish_at(2100, "c");
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "NA", "c"}));
}

TEST_F(TextFilterTest, TimeoutSendsTheLastValueAgain) {
  TimeoutFilterLast timeout(1000);
  this->sensor_.add_filter(&timeout);
  this->publish_at(1000, "a");
  loop_at(timeout, 1999);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a"}));
  loop_at(timeout, 2000);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "a"}));
}

TEST_F(TextFilterTest, TimeoutRearmsOnEachValueAndFiresOnce) {
  TimeoutFilterLast timeout(1000);
  this->sensor_.add_filter(&timeout);
  this->publish_at(1000, "a");
  this->publish_at(1800, "b");
  loop_at(timeout, 2500);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "b"}));
  EXPECT_FALSE(timeout.is_idle());
  loop_at(timeout, 2800);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "b", "b"}));
  EXPECT_TRUE(timeout.is_idle());
}

TEST_F(TextFilterTest, TimeoutSendsTheConfiguredValue) {
  TimeoutFilterConfigured timeout(1000, "Offline");
  this->sensor_.add_filter(&timeout);
  this->publish_at(1000, "a");
  loop_at(timeout, 2000);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "Offline"}));
}

// The default the ld2450 position sensor uses: the final value before a pause gets through the throttle
TEST_F(TextFilterTest, TimeoutThenThrottlePublishesTheHeldValue) {
  TimeoutFilterLast timeout(1000);
  ThrottleWithPriorityFilter throttle(1000, PRIORITIZED);
  this->sensor_.add_filters({&timeout, &throttle});
  this->publish_at(1000, "a");
  this->publish_at(1200, "b");
  loop_at(timeout, 2200);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "b"}));
}

TEST_F(TextFilterTest, CombinedTimeoutThrottleMatchesTheChain) {
  TimeoutThrottleFilter combined(1000, PRIORITIZED);
  this->sensor_.add_filter(&combined);
  this->publish_at(1000, "a");
  this->publish_at(1200, "b");
  this->publish_at(1300, "NA");
  this->publish_at(1400, "c");
  loop_at(combined, 2400);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "NA", "c"}));
}

}  // namespace esphome::text_sensor::testing
