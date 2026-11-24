#ifdef USE_HOST

#include <gtest/gtest.h>

#include "esphome/components/current_based/current_based_cover.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

#include <cmath>

namespace esphome::current_based::testing {

using cover::COVER_CLOSED;
using cover::COVER_OPEN;
using cover::COVER_OPERATION_CLOSING;
using cover::COVER_OPERATION_IDLE;
using cover::COVER_OPERATION_OPENING;

static constexpr uint32_t CONFIGURED_DURATION = 10000;
static constexpr double TOLERANCE = 100;

namespace {
class CurrentBasedCoverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    host::setup_preferences();
    host::host_preferences->reset();
  }

  void setup_cover_(CurrentBasedCover &cover, bool learn_durations, uint32_t duration = CONFIGURED_DURATION) {
    cover.set_open_sensor(&this->open_current_);
    cover.set_close_sensor(&this->close_current_);
    cover.set_open_moving_current_threshold(0.1f);
    cover.set_close_moving_current_threshold(0.1f);
    cover.set_open_duration(duration);
    cover.set_close_duration(duration);
    cover.set_start_sensing_delay(0);
    cover.set_learn_durations(learn_durations);
    cover.setup();
  }

  void draw_current_(CurrentBasedCover &cover) {
    const bool opening = cover.current_operation == COVER_OPERATION_OPENING;
    this->open_current_.publish_state(opening ? 1.0f : 0.0f);
    this->close_current_.publish_state(opening ? 0.0f : 1.0f);
    delay(2);
    cover.loop();
  }

  void stop_drawing_current_(CurrentBasedCover &cover, uint32_t travel_ms) {
    this->open_current_.publish_state(0.0f);
    this->close_current_.publish_state(0.0f);
    delay(2);
    LoopBlockingGuard guard{nullptr, nullptr, millis() + travel_ms};
    cover.loop();
    ASSERT_EQ(cover.current_operation, COVER_OPERATION_IDLE);
  }

  void travel_(CurrentBasedCover &cover, uint32_t travel_ms) {
    this->draw_current_(cover);
    this->stop_drawing_current_(cover, travel_ms);
  }

  void close_(CurrentBasedCover &cover, uint32_t travel_ms) {
    cover.make_call().set_command_close().perform();
    this->travel_(cover, travel_ms);
    ASSERT_EQ(cover.position, COVER_CLOSED);
  }

  void open_(CurrentBasedCover &cover, uint32_t travel_ms) {
    cover.make_call().set_command_open().perform();
    this->travel_(cover, travel_ms);
    ASSERT_EQ(cover.position, COVER_OPEN);
  }

  sensor::Sensor open_current_;
  sensor::Sensor close_current_;
};
}  // namespace

TEST_F(CurrentBasedCoverTest, LearnsDurationsOfTravelsBetweenEnds) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);

  this->close_(cover, 1000);
  EXPECT_EQ(cover.get_close_duration(), CONFIGURED_DURATION);

  this->open_(cover, 7000);
  EXPECT_NEAR(cover.get_open_duration(), 7000, TOLERANCE);

  this->close_(cover, 6000);
  EXPECT_NEAR(cover.get_close_duration(), 6000, TOLERANCE);
}

TEST_F(CurrentBasedCoverTest, DoesNotLearnWhenDisabled) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, false);

  this->close_(cover, 1000);
  this->open_(cover, 7000);
  this->close_(cover, 6000);

  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
  EXPECT_EQ(cover.get_close_duration(), CONFIGURED_DURATION);
}

TEST_F(CurrentBasedCoverTest, DoesNotLearnWhenNoMotorCurrentWasSeen) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);
  this->close_(cover, 1000);

  cover.make_call().set_command_open().perform();
  this->stop_drawing_current_(cover, 7000);

  EXPECT_EQ(cover.position, COVER_OPEN);
  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
}

struct DurationCase {
  uint32_t configured;
  uint32_t first;
  uint32_t second;
};

class CurrentBasedCoverSmallChangeTest : public CurrentBasedCoverTest,
                                         public ::testing::WithParamInterface<DurationCase> {};

TEST_P(CurrentBasedCoverSmallChangeTest, KeepsDurationWhenChangeIsSmall) {
  const DurationCase &param = GetParam();
  CurrentBasedCover cover;
  this->setup_cover_(cover, true, param.configured);
  this->close_(cover, 1000);
  this->open_(cover, param.first);
  const uint32_t learned = cover.get_open_duration();
  ASSERT_NEAR(learned, param.first, TOLERANCE);
  this->close_(cover, param.first);

  this->open_(cover, param.second);

  EXPECT_EQ(cover.get_open_duration(), learned);
}

INSTANTIATE_TEST_SUITE_P(SmallChanges, CurrentBasedCoverSmallChangeTest,
                         ::testing::Values(DurationCase{10000, 7000, 7300},      // under 5% and under 1 s
                                           DurationCase{10000, 7000, 7900},      // over 5%, under 1 s
                                           DurationCase{40000, 30000, 31200}));  // over 1 s, under 5%

class CurrentBasedCoverImplausibleTest : public CurrentBasedCoverTest,
                                         public ::testing::WithParamInterface<uint32_t> {};

TEST_P(CurrentBasedCoverImplausibleTest, DoesNotLearnDurationFarFromConfigured) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);
  this->close_(cover, 1000);

  this->open_(cover, GetParam());

  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
}

INSTANTIATE_TEST_SUITE_P(Implausible, CurrentBasedCoverImplausibleTest,
                         ::testing::Values(CONFIGURED_DURATION / 2 - 200, CONFIGURED_DURATION * 2 + 200));

TEST_F(CurrentBasedCoverTest, DoesNotLearnTravelThatWasReversed) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true, 4000);
  this->close_(cover, 1000);

  cover.make_call().set_command_open().perform();
  this->draw_current_(cover);
  delay(120);  // NOLINT
  cover.loop();
  cover.make_call().set_command_close().perform();
  ASSERT_EQ(cover.current_operation, COVER_OPERATION_CLOSING);
  this->travel_(cover, 6000);

  EXPECT_EQ(cover.position, COVER_CLOSED);
  EXPECT_EQ(cover.get_close_duration(), 4000);
}

TEST_F(CurrentBasedCoverTest, DoesNotLearnFromEndReachedWithoutMotorCurrent) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);

  cover.make_call().set_command_open().perform();
  this->stop_drawing_current_(cover, 1000);
  ASSERT_EQ(cover.position, COVER_OPEN);
  this->close_(cover, 6000);

  EXPECT_EQ(cover.get_close_duration(), CONFIGURED_DURATION);
}

TEST_F(CurrentBasedCoverTest, DoesNotLearnFromInvalidCurrentReading) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);
  this->close_(cover, 1000);

  cover.make_call().set_command_open().perform();
  this->draw_current_(cover);
  this->open_current_.publish_state(NAN);
  delay(2);
  {
    LoopBlockingGuard guard{nullptr, nullptr, millis() + 7000};
    cover.loop();
  }
  ASSERT_EQ(cover.position, COVER_OPEN);
  this->close_(cover, 6000);

  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
  EXPECT_EQ(cover.get_close_duration(), CONFIGURED_DURATION);
}

TEST_F(CurrentBasedCoverTest, DoesNotLearnFromInfiniteCurrentReading) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);
  this->close_(cover, 1000);

  cover.make_call().set_command_open().perform();
  this->open_current_.publish_state(INFINITY);
  delay(2);
  cover.loop();
  ASSERT_EQ(cover.current_operation, COVER_OPERATION_OPENING);
  this->stop_drawing_current_(cover, 7000);
  ASSERT_EQ(cover.position, COVER_OPEN);
  this->close_(cover, 6000);

  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
  EXPECT_EQ(cover.get_close_duration(), CONFIGURED_DURATION);
}

enum class Interruption : uint8_t { INTERRUPTION_STOP, INTERRUPTION_TOGGLE, INTERRUPTION_MALFUNCTION };

class CurrentBasedCoverInterruptionTest : public CurrentBasedCoverTest,
                                          public ::testing::WithParamInterface<Interruption> {};

TEST_P(CurrentBasedCoverInterruptionTest, DoesNotLearnTravelThatWasInterrupted) {
  CurrentBasedCover cover;
  this->setup_cover_(cover, true);
  this->close_(cover, 1000);

  cover.make_call().set_command_open().perform();
  this->draw_current_(cover);
  switch (GetParam()) {
    case Interruption::INTERRUPTION_STOP:
      cover.make_call().set_command_stop().perform();
      break;
    case Interruption::INTERRUPTION_TOGGLE:
      cover.make_call().set_command_toggle().perform();
      break;
    case Interruption::INTERRUPTION_MALFUNCTION:
      this->close_current_.publish_state(1.0f);
      cover.loop();
      break;
  }
  ASSERT_EQ(cover.current_operation, COVER_OPERATION_IDLE);
  this->open_(cover, 7000);

  EXPECT_EQ(cover.get_open_duration(), CONFIGURED_DURATION);
}

INSTANTIATE_TEST_SUITE_P(Interruptions, CurrentBasedCoverInterruptionTest,
                         ::testing::Values(Interruption::INTERRUPTION_STOP, Interruption::INTERRUPTION_TOGGLE,
                                           Interruption::INTERRUPTION_MALFUNCTION));

TEST_F(CurrentBasedCoverTest, RestoresLearnedDurationsAfterReboot) {
  {
    CurrentBasedCover cover;
    this->setup_cover_(cover, true);
    this->close_(cover, 1000);
    this->open_(cover, 7000);
  }

  CurrentBasedCover rebooted;
  this->setup_cover_(rebooted, true);

  EXPECT_NEAR(rebooted.get_open_duration(), 7000, TOLERANCE);
  EXPECT_EQ(rebooted.get_close_duration(), CONFIGURED_DURATION);
}

TEST_F(CurrentBasedCoverTest, DiscardsLearnedDurationsWhenConfiguredDurationsChange) {
  {
    CurrentBasedCover cover;
    this->setup_cover_(cover, true);
    this->close_(cover, 1000);
    this->open_(cover, 7000);
  }

  CurrentBasedCover reconfigured;
  this->setup_cover_(reconfigured, true, 30000);

  EXPECT_EQ(reconfigured.get_open_duration(), 30000);
  EXPECT_EQ(reconfigured.get_close_duration(), 30000);
}

TEST_F(CurrentBasedCoverTest, DoesNotRestoreLearnedDurationsWhenConfiguredDurationsChangeBack) {
  {
    CurrentBasedCover cover;
    this->setup_cover_(cover, true);
    this->close_(cover, 1000);
    this->open_(cover, 7000);
  }
  {
    CurrentBasedCover reconfigured;
    this->setup_cover_(reconfigured, true, 30000);
  }

  CurrentBasedCover reverted;
  this->setup_cover_(reverted, true);

  EXPECT_EQ(reverted.get_open_duration(), CONFIGURED_DURATION);
  EXPECT_EQ(reverted.get_close_duration(), CONFIGURED_DURATION);
}

}  // namespace esphome::current_based::testing

#endif
