// on_time runs once per second: each check fires for the current second and for any seconds skipped since the
// last check, unless the clock jumped more than 15 minutes, which is treated as a time sync.

#include <gtest/gtest.h>

#include "esphome/components/time/real_time_clock.h"

namespace esphome::time::testing {

static constexpr time_t MONDAY_072958 = 1767598198;  // 2026-01-05 07:29:58 UTC, a Monday
static constexpr time_t MONDAY_235958 = 1767657598;

class FakeClock : public RealTimeClock {
 public:
  using RealTimeClock::check_time_;
  time_t timestamp_now() override { return this->epoch; }
  void update() override {}
  time_t epoch{0};
};

class OnTimeTest : public ::testing::Test {
 protected:
  // Stands in for the check code generation writes for each on_time automation.
  void match(bool (*check)(const ESPTime &, uint8_t), uint8_t arg) {
    this->check_ = check;
    this->arg_ = arg;
    this->clock_.add_on_time_callback([this](const ESPTime &time) {
      if (this->check_(time, this->arg_))
        this->fires_++;
    });
  }
  void match_every_second() {
    this->match([](const ESPTime &, uint8_t) { return true; }, 0);
  }
  void match_second(uint8_t second) {
    this->match([](const ESPTime &time, uint8_t second) { return time.second == second; }, second);
  }
  void match_midnight_on(uint8_t day_of_week) {
    this->match(
        [](const ESPTime &time, uint8_t day_of_week) {
          return time.second == 0 && time.minute == 0 && time.hour == 0 && time.day_of_week == day_of_week;
        },
        day_of_week);
  }
  // One pass of the 1 s check with the clock at `epoch`; returns how many times on_time fired.
  int tick(time_t epoch) {
    this->clock_.epoch = epoch;
    this->fires_ = 0;
    this->clock_.check_time_();
    return this->fires_;
  }

  FakeClock clock_;
  bool (*check_)(const ESPTime &, uint8_t){nullptr};
  uint8_t arg_{0};
  int fires_{0};
};

TEST_F(OnTimeTest, FiresOnlyOnMatchingSecond) {
  this->match_second(0);
  EXPECT_EQ(this->tick(MONDAY_072958), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 1), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 2), 1);  // 07:30:00
  EXPECT_EQ(this->tick(MONDAY_072958 + 3), 0);
}

TEST_F(OnTimeTest, FirstValidTimeFiresWhenItMatches) {
  this->match_second(58);
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
}

TEST_F(OnTimeTest, SameSecondFiresOnce) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
  EXPECT_EQ(this->tick(MONDAY_072958), 0);
}

TEST_F(OnTimeTest, InvalidTimeNeverFires) {
  this->match_every_second();
  EXPECT_EQ(this->tick(0), 0);  // 1970 is before the earliest valid year
  EXPECT_EQ(this->tick(1), 0);
  // The invalid ticks are not remembered, so the first valid time does not catch up from 1970.
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
}

TEST_F(OnTimeTest, SkippedSecondsAreCaughtUp) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
  EXPECT_EQ(this->tick(MONDAY_072958 + 5), 5);  // four skipped seconds and the current one
}

TEST_F(OnTimeTest, SkippedMatchingSecondIsCaughtUp) {
  this->match_second(0);
  EXPECT_EQ(this->tick(MONDAY_072958), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 4), 1);  // 07:30:00 was skipped
  EXPECT_EQ(this->tick(MONDAY_072958 + 5), 0);
}

TEST_F(OnTimeTest, CatchUpUpToFifteenMinutes) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
  EXPECT_EQ(this->tick(MONDAY_072958 + 900), 900);
}

TEST_F(OnTimeTest, JumpAheadOverFifteenMinutesSkipsCurrentSecond) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
  EXPECT_EQ(this->tick(MONDAY_072958 + 901), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 902), 1);
}

TEST_F(OnTimeTest, SmallJumpBackWaitsUntilTimeCatchesUp) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958 + 10), 1);
  EXPECT_EQ(this->tick(MONDAY_072958), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 10), 0);
  EXPECT_EQ(this->tick(MONDAY_072958 + 11), 1);
}

TEST_F(OnTimeTest, LargeJumpBackFiresForNewTime) {
  this->match_every_second();
  EXPECT_EQ(this->tick(MONDAY_072958 + 1000), 1);
  EXPECT_EQ(this->tick(MONDAY_072958), 1);
  EXPECT_EQ(this->tick(MONDAY_072958 + 1), 1);
}

TEST_F(OnTimeTest, CatchUpMatchesTheSkippedSecondsDate) {
  this->match_midnight_on(3);  // Tuesday
  EXPECT_EQ(this->tick(MONDAY_235958), 0);
  EXPECT_EQ(this->tick(MONDAY_235958 + 4), 1);  // Tuesday 00:00:00 was skipped
}

TEST_F(OnTimeTest, CatchUpSkipsOtherDays) {
  this->match_midnight_on(2);  // Monday
  EXPECT_EQ(this->tick(MONDAY_235958), 0);
  EXPECT_EQ(this->tick(MONDAY_235958 + 4), 0);
}

}  // namespace esphome::time::testing
