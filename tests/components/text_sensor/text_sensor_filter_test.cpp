#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/text_sensor/filter.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

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
  TimeoutFilter timeout(1000, nullptr);
  this->sensor_.add_filter(&timeout);
  this->sensor_.publish_state("a");
  const uint32_t start = millis();
  {
    LoopBlockingGuard guard{nullptr, nullptr, start + 999};
    timeout.loop();
  }
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a"}));
  {
    LoopBlockingGuard guard{nullptr, nullptr, start + 1000};
    timeout.loop();
  }
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "a"}));
}

TEST_F(TextFilterTest, TimeoutSendsTheConfiguredValue) {
  TimeoutFilter timeout(1000, "Offline");
  this->sensor_.add_filter(&timeout);
  this->sensor_.publish_state("a");
  LoopBlockingGuard guard{nullptr, nullptr, millis() + 1000};
  timeout.loop();
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "Offline"}));
}

// The default the ld2450 position sensor uses: the final value before a pause gets through the throttle
TEST_F(TextFilterTest, TimeoutThenThrottlePublishesTheHeldValue) {
  TimeoutFilter timeout(1000, nullptr);
  ThrottleWithPriorityFilter throttle(1000, PRIORITIZED);
  this->sensor_.add_filters({&timeout, &throttle});
  const uint32_t start = millis();
  this->publish_at(start, "a");
  this->publish_at(start + 200, "b");
  {
    LoopBlockingGuard guard{nullptr, nullptr, millis() + 1000};
    timeout.loop();
  }
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"a", "b"}));
}

}  // namespace esphome::text_sensor::testing
