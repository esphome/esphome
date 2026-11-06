#include <gtest/gtest.h>
#include <functional>
#include <utility>

#include "esphome/components/binary_sensor/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/hal.h"

namespace esphome::binary_sensor::testing {

// The click detectors read the real millis(), so every wait is at least 80 ms away from a window edge
static constexpr uint32_t MIN_MS = 100;
static constexpr uint32_t MAX_MS = 300;
static constexpr uint32_t SHORT_MS = 20;
static constexpr uint32_t INSIDE_MS = 200;
static constexpr uint32_t LONG_MS = 450;

static int fired = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class ClickTriggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fired = 0;
    this->sensor_.publish_state(false);
  }

  // As codegen builds on_click and on_double_click
  void add_click_(uint32_t min_length = MIN_MS, uint32_t max_length = MAX_MS, BinarySensor *sensor = nullptr) {
    this->watch_(this->click_, min_length, max_length, sensor != nullptr ? *sensor : this->sensor_);
  }
  void add_double_click_(std::function<void()> on_fire = count_fire) {
    this->watch_(this->double_click_, MIN_MS, MAX_MS, this->sensor_, std::move(on_fire));
  }
  template<typename D>
  void watch_(D &detector, uint32_t min_length, uint32_t max_length, BinarySensor &sensor,
              std::function<void()> on_fire = count_fire) {
    sensor.add_on_state_callback([&detector, min_length, max_length, on_fire = std::move(on_fire)](bool state) {
      if (detector.check(state, min_length, max_length))
        on_fire();
    });
  }
  static void count_fire() { fired++; }

  void press_for_(uint32_t length) {
    this->sensor_.publish_state(true);
    delay(length);
    this->sensor_.publish_state(false);
  }

  BinarySensor sensor_;
  ClickDetector click_;
  DoubleClickDetector double_click_;
};

TEST(ClickMatchInterval, BoundsAreInclusive) {
  EXPECT_FALSE(match_interval(50, 350, 49));
  EXPECT_TRUE(match_interval(50, 350, 50));
  EXPECT_TRUE(match_interval(50, 350, 350));
  EXPECT_FALSE(match_interval(50, 350, 351));
  // A max of 0 means no maximum
  EXPECT_TRUE(match_interval(50, 0, UINT32_MAX));
  EXPECT_FALSE(match_interval(50, 0, 49));
}

TEST_F(ClickTriggerTest, ClickInsideWindowFiresOnRelease) {
  this->add_click_();
  this->sensor_.publish_state(true);
  delay(INSIDE_MS);
  EXPECT_EQ(fired, 0);
  this->sensor_.publish_state(false);
  EXPECT_EQ(fired, 1);
  this->press_for_(INSIDE_MS);
  EXPECT_EQ(fired, 2);
}

TEST_F(ClickTriggerTest, ClickOutsideWindowDoesNotFire) {
  this->add_click_();
  this->press_for_(SHORT_MS);
  this->press_for_(LONG_MS);
  EXPECT_EQ(fired, 0);
}

TEST_F(ClickTriggerTest, ClickWithoutMaximumFiresOnLongPress) {
  this->add_click_(0, 0);
  this->press_for_(LONG_MS);
  EXPECT_EQ(fired, 1);
}

TEST_F(ClickTriggerTest, RepeatedPressDoesNotRestartClick) {
  this->add_click_();
  this->sensor_.publish_state(true);
  delay(INSIDE_MS);
  // Not a state change, so the click is now 400 ms long
  this->sensor_.publish_state(true);
  delay(INSIDE_MS);
  this->sensor_.publish_state(false);
  EXPECT_EQ(fired, 0);
}

TEST_F(ClickTriggerTest, ClickSeesFilteredState) {
  InvertFilter invert;
  this->sensor_.add_filter(&invert);
  this->add_click_();
  // Inverted: false presses and true releases
  this->sensor_.publish_state(false);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
  this->sensor_.publish_state(false);
  delay(LONG_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
}

TEST_F(ClickTriggerTest, InitialPressIsNotAClick) {
  BinarySensor sensor;
  sensor.set_trigger_on_initial_state(false);
  this->add_click_(MIN_MS, MAX_MS, &sensor);
  // As codegen sets it: the first state skips the callback, so the click is timed from 0
  sensor.publish_state(true);
  delay(INSIDE_MS);
  sensor.publish_state(false);
  EXPECT_EQ(fired, 0);
}

TEST_F(ClickTriggerTest, DoubleClickFiresOnSecondPress) {
  this->add_double_click_();
  this->press_for_(INSIDE_MS);
  delay(INSIDE_MS);
  EXPECT_EQ(fired, 0);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
  // The second press is not timed
  delay(LONG_MS);
  this->sensor_.publish_state(false);
  EXPECT_EQ(fired, 1);
}

TEST_F(ClickTriggerTest, DoubleClickNeedsFirstPressInWindow) {
  this->add_double_click_();
  this->press_for_(SHORT_MS);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 0);
  this->sensor_.publish_state(false);
  this->press_for_(LONG_MS);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 0);
}

TEST_F(ClickTriggerTest, DoubleClickNeedsGapInWindow) {
  this->add_double_click_();
  this->press_for_(INSIDE_MS);
  delay(SHORT_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 0);
  this->sensor_.publish_state(false);
  this->press_for_(INSIDE_MS);
  delay(LONG_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 0);
}

TEST_F(ClickTriggerTest, DoubleClickWindowSlidesOverEdges) {
  this->add_double_click_();
  // A gap too long, then a press and gap in the window: the last release and press match
  this->press_for_(INSIDE_MS);
  delay(LONG_MS);
  this->press_for_(INSIDE_MS);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
}

TEST_F(ClickTriggerTest, DoubleClickResetsAfterFiring) {
  this->add_double_click_();
  this->press_for_(INSIDE_MS);
  delay(INSIDE_MS);
  this->press_for_(INSIDE_MS);
  EXPECT_EQ(fired, 1);
  // The release after a double click does not pair with its edges: the next press only starts one
  delay(INSIDE_MS);
  this->press_for_(INSIDE_MS);
  EXPECT_EQ(fired, 1);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 2);
}

TEST_F(ClickTriggerTest, DoubleClickDoesNotFireTwiceWithoutReset) {
  this->add_double_click_();
  this->press_for_(INSIDE_MS);
  delay(INSIDE_MS);
  this->press_for_(SHORT_MS);
  EXPECT_EQ(fired, 1);
  // Without the reset, the first release and the short second press would pair with this gap
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
}

TEST_F(ClickTriggerTest, DoubleClickKeepsPressPublishedByItsActions) {
  // The times reset before the actions run, so a press the first action publishes is kept
  this->add_double_click_([this]() {
    if (++fired == 1) {
      this->sensor_.publish_state(false);
      this->sensor_.publish_state(true);
    }
  });
  this->press_for_(INSIDE_MS);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 1);
  delay(INSIDE_MS);
  this->sensor_.publish_state(false);
  delay(INSIDE_MS);
  this->sensor_.publish_state(true);
  EXPECT_EQ(fired, 2);
}

}  // namespace esphome::binary_sensor::testing
