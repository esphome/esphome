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
  void publish_at_(uint32_t now, const char *value) {
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
  this->publish_at_(1000, "a");
  this->publish_at_(1500, "b");
  this->publish_at_(2000, "c");
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "c"}));
}

TEST_F(TextFilterTest, PrioritizedValuePassesAtOnce) {
  ThrottleWithPriorityFilter throttle(1000, PRIORITIZED);
  this->sensor_.add_filter(&throttle);
  this->publish_at_(1000, "a");
  this->publish_at_(1100, "NA");
  this->publish_at_(1200, "b");
  this->publish_at_(2100, "c");
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "NA", "c"}));
}

TEST_F(TextFilterTest, TimeoutRearmsOnEachValueAndFiresOnce) {
  TimeoutFilterLast timeout(1000);
  this->sensor_.add_filter(&timeout);
  this->publish_at_(1000, "a");
  this->publish_at_(1800, "b");
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
  this->publish_at_(1000, "a");
  loop_at(timeout, 2000);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "Offline"}));
}

// Feeds the same values to the timeout + throttle chain and to the merged filter
static std::vector<std::string> run_sequence(std::initializer_list<Filter *> filters, Component &timeout) {
  TextSensor sensor;
  std::vector<std::string> seen;
  sensor.add_on_state_callback([&seen](const std::string &state) { seen.push_back(state); });
  sensor.add_filters(filters);
  const auto publish_at = [&sensor](uint32_t now, const char *value) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    sensor.publish_state(value);
  };
  const auto loop_at = [&timeout](uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    timeout.loop();
  };
  publish_at(1000, "a");
  publish_at(1200, "b");  // held by the throttle, sent once updates pause
  loop_at(2200);
  publish_at(2300, "c");
  publish_at(2400, "NA");  // a priority value passes at once
  publish_at(2500, "d");
  loop_at(3500);
  return seen;
}

TEST(TextFilterChain, CombinedTimeoutThrottleMatchesTheChain) {
  TimeoutFilterLast timeout(1000);
  ThrottleWithPriorityFilter throttle(1000, PRIORITIZED);
  const auto chain = run_sequence({&timeout, &throttle}, timeout);
  TimeoutThrottleFilter combined(1000, PRIORITIZED);
  EXPECT_EQ(run_sequence({&combined}, combined), chain);
  EXPECT_EQ(chain, (std::vector<std::string>{"a", "b", "NA", "d"}));
}

TEST_F(TextFilterTest, CombinedWithoutPriorityIsAPlainThrottle) {
  TimeoutThrottleFilter combined(1000, nullptr);
  this->sensor_.add_filter(&combined);
  this->publish_at_(1000, "a");
  this->publish_at_(1300, "NA");
  loop_at(combined, 2300);
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "NA"}));
}

}  // namespace esphome::text_sensor::testing
