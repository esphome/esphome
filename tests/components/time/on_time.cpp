// on_time runs once per second: each check fires for the current second and for any seconds skipped since the
// last check, unless the clock jumped more than 15 minutes, which is treated as a time sync.

#include <gtest/gtest.h>

// CronTrigger is final; drop that here so the test can reach check_time_() without the scheduler.
#define final
#include "esphome/components/time/automation.h"
#undef final

namespace esphome::time::testing {

static constexpr time_t MONDAY_072958 = 1767598198;  // 2026-01-05 07:29:58 UTC, a Monday
static constexpr time_t MONDAY_235958 = 1767657598;

class FakeClock : public RealTimeClock {
 public:
  time_t timestamp_now() override { return this->epoch; }
  void update() override {}
  time_t epoch{0};
};

class CountAction : public Action<> {
 public:
  explicit CountAction(int *count) : count_(count) {}

 protected:
  void play() override { (*this->count_)++; }
  int *count_;
};

class TestCronTrigger : public CronTrigger {
 public:
  using CronTrigger::check_time_;
  using CronTrigger::CronTrigger;
};

class OnTimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Other tests leave their zone set; these times are UTC.
    this->saved_tz_ = get_global_tz();
    set_global_tz(ParsedTimezone{});
    this->automation_.add_action(&this->count_);
  }
  void TearDown() override { set_global_tz(this->saved_tz_); }

  void match_all_fields_except_seconds_() {
    for (uint8_t i = 0; i < 60; i++)
      this->trigger_.add_minute(i);
    for (uint8_t i = 0; i < 24; i++)
      this->trigger_.add_hour(i);
    for (uint8_t i = 1; i <= 31; i++)
      this->trigger_.add_day_of_month(i);
    for (uint8_t i = 1; i <= 12; i++)
      this->trigger_.add_month(i);
    for (uint8_t i = 1; i <= 7; i++)
      this->trigger_.add_day_of_week(i);
  }
  void match_every_second_() {
    this->match_all_fields_except_seconds_();
    for (uint8_t i = 0; i <= 60; i++)
      this->trigger_.add_second(i);
  }
  void match_second_(uint8_t second) {
    this->match_all_fields_except_seconds_();
    this->trigger_.add_second(second);
  }
  void match_midnight_on_(uint8_t day_of_week) {
    this->trigger_.add_second(0);
    this->trigger_.add_minute(0);
    this->trigger_.add_hour(0);
    for (uint8_t i = 1; i <= 31; i++)
      this->trigger_.add_day_of_month(i);
    for (uint8_t i = 1; i <= 12; i++)
      this->trigger_.add_month(i);
    this->trigger_.add_day_of_week(day_of_week);
  }
  // One pass of the 1 s check with the clock at `epoch`; returns how many times on_time fired.
  int tick_(time_t epoch) {
    this->clock_.epoch = epoch;
    this->fires_ = 0;
    this->trigger_.check_time_();
    return this->fires_;
  }

  ParsedTimezone saved_tz_{};
  FakeClock clock_;
  TestCronTrigger trigger_{&this->clock_};
  Automation<> automation_{&this->trigger_};
  int fires_{0};
  CountAction count_{&this->fires_};
};

TEST_F(OnTimeTest, FiresOnlyOnMatchingSecond) {
  this->match_second_(0);
  EXPECT_EQ(this->tick_(MONDAY_072958), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 1), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 2), 1);  // 07:30:00
  EXPECT_EQ(this->tick_(MONDAY_072958 + 3), 0);
}

TEST_F(OnTimeTest, FirstValidTimeFiresWhenItMatches) {
  this->match_second_(58);
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
}

TEST_F(OnTimeTest, SameSecondFiresOnce) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958), 0);
}

TEST_F(OnTimeTest, InvalidTimeNeverFires) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(0), 0);  // 1970 is before the earliest valid year
  EXPECT_EQ(this->tick_(1), 0);
  // The invalid ticks are not remembered, so the first valid time does not catch up from 1970.
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
}

TEST_F(OnTimeTest, SkippedSecondsAreCaughtUp) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 5), 5);  // four skipped seconds and the current one
}

TEST_F(OnTimeTest, SkippedMatchingSecondIsCaughtUp) {
  this->match_second_(0);
  EXPECT_EQ(this->tick_(MONDAY_072958), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 4), 1);  // 07:30:00 was skipped
  EXPECT_EQ(this->tick_(MONDAY_072958 + 5), 0);
}

TEST_F(OnTimeTest, CatchUpUpToFifteenMinutes) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 900), 900);
}

TEST_F(OnTimeTest, JumpAheadOverFifteenMinutesSkipsCurrentSecond) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 901), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 902), 1);
}

TEST_F(OnTimeTest, SmallJumpBackWaitsUntilTimeCatchesUp) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958 + 10), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 10), 0);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 11), 1);
}

TEST_F(OnTimeTest, LargeJumpBackFiresForNewTime) {
  this->match_every_second_();
  EXPECT_EQ(this->tick_(MONDAY_072958 + 1000), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958), 1);
  EXPECT_EQ(this->tick_(MONDAY_072958 + 1), 1);
}

TEST_F(OnTimeTest, CatchUpMatchesTheSkippedSecondsDate) {
  this->match_midnight_on_(3);  // Tuesday
  EXPECT_EQ(this->tick_(MONDAY_235958), 0);
  EXPECT_EQ(this->tick_(MONDAY_235958 + 4), 1);  // Tuesday 00:00:00 was skipped
}

TEST_F(OnTimeTest, CatchUpSkipsOtherDays) {
  this->match_midnight_on_(2);  // Monday
  EXPECT_EQ(this->tick_(MONDAY_235958), 0);
  EXPECT_EQ(this->tick_(MONDAY_235958 + 4), 0);
}

}  // namespace esphome::time::testing
