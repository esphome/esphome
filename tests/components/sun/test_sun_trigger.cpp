#include <gtest/gtest.h>

#include <cmath>
#include <ctime>
#include <list>

#include "esphome/components/sun/sun.h"
#include "esphome/core/base_automation.h"

namespace esphome::sun::testing {

// 2024-06-21 00:00:00 UTC; at the Eiffel Tower the sun rises around 03:47 and sets around 19:58 UTC.
static constexpr time_t DAY_START = 1718928000;
static constexpr time_t MINUTE = 60;
static constexpr time_t NOON = DAY_START + 12 * 3600;
static constexpr time_t MORNING = DAY_START + 5 * 3600;
static constexpr time_t EVENING = DAY_START + 19 * 3600;
static constexpr double LATITUDE = 48.8584;
static constexpr double LONGITUDE = 2.2945;
static constexpr double DEFAULT_ELEVATION = -0.83333;

class FakeClock : public time::RealTimeClock {
 public:
  time_t timestamp_now() override { return this->now; }
  void update() override {}
  time_t now{DAY_START};
};

struct Record {
  int count{0};
  time_t last_fired{0};
};

struct Watch {
  SunTrigger trigger;
  Automation<> automation{&this->trigger};
  LambdaAction<> action;
  explicit Watch(std::function<void()> &&on_fire) : action(std::move(on_fire)) {
    this->automation.add_action(&this->action);
  }
};

class SunTriggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sun_.set_time(&this->clock_);
    this->sun_.set_latitude(LATITUDE);
    this->sun_.set_longitude(LONGITUDE);
  }

  Record &add(bool sunrise, double elevation) {
    auto &record = this->records_.emplace_back();
    auto &watch = this->watches_.emplace_back([this, &record]() {
      record.count++;
      record.last_fired = this->clock_.now;
    });
    watch.trigger.set_parent(&this->sun_);
    watch.trigger.set_sunrise(sunrise);
    watch.trigger.set_elevation(elevation);
    return record;
  }

  // One 60 s poll at time t.
  void tick(time_t t) {
    this->clock_.now = t;
    for (auto &watch : this->watches_)
      watch.trigger.update();
  }

  double elevation_at(time_t t) {
    this->clock_.now = t;
    return this->sun_.elevation();
  }

  // Elevation set to exactly the value at t: no fire when it lands there, fires on the next poll.
  void expect_landing_fires_next_poll(bool sunrise, time_t t) {
    auto &record = this->add(sunrise, this->elevation_at(t));
    this->tick(t - MINUTE);
    this->tick(t);
    EXPECT_EQ(record.count, 0);
    this->tick(t + MINUTE);
    EXPECT_EQ(record.count, 1);
    EXPECT_EQ(record.last_fired, t + MINUTE);
  }

  // The sun passes the elevation in the other direction around t.
  void expect_other_direction_ignored(bool sunrise, time_t t) {
    auto &record = this->add(sunrise, this->elevation_at(t));
    this->tick(t - MINUTE);
    this->tick(t + MINUTE);
    EXPECT_EQ(record.count, 0);
  }

  FakeClock clock_;
  Sun sun_;
  std::list<Record> records_;
  std::list<Watch> watches_;
};

TEST_F(SunTriggerTest, FullDayFiresEachTriggerOnce) {
  auto &sunrise = this->add(true, DEFAULT_ELEVATION);
  auto &sunrise_high = this->add(true, 5.0);
  auto &sunset = this->add(false, DEFAULT_ELEVATION);
  auto &sunset_high = this->add(false, 5.0);

  for (time_t t = DAY_START; t < DAY_START + 24 * 3600; t += MINUTE)
    this->tick(t);

  EXPECT_EQ(sunrise.count, 1);
  EXPECT_EQ(sunrise_high.count, 1);
  EXPECT_EQ(sunset.count, 1);
  EXPECT_EQ(sunset_high.count, 1);
  EXPECT_LT(sunrise.last_fired, sunrise_high.last_fired);
  EXPECT_LT(sunrise_high.last_fired, NOON);
  EXPECT_GT(sunset_high.last_fired, NOON);
  EXPECT_LT(sunset_high.last_fired, sunset.last_fired);

  // Each fired on the first poll past its elevation.
  EXPECT_LE(this->elevation_at(sunrise.last_fired - MINUTE), DEFAULT_ELEVATION);
  EXPECT_GT(this->elevation_at(sunrise.last_fired), DEFAULT_ELEVATION);
  EXPECT_GE(this->elevation_at(sunset.last_fired - MINUTE), DEFAULT_ELEVATION);
  EXPECT_LT(this->elevation_at(sunset.last_fired), DEFAULT_ELEVATION);
}

TEST_F(SunTriggerTest, FirstPollNeverFires) {
  const time_t start = NOON - 3 * 3600;
  const double current = this->elevation_at(start);
  // Already past both thresholds on the first poll.
  auto &sunrise = this->add(true, current - 1.0);
  auto &sunset = this->add(false, current + 1.0);

  this->tick(start);
  EXPECT_EQ(sunrise.count, 0);
  EXPECT_EQ(sunset.count, 0);

  // The morning keeps rising: no new crossing for either.
  this->tick(start + MINUTE);
  EXPECT_EQ(sunrise.count, 0);
  EXPECT_EQ(sunset.count, 0);
}

TEST_F(SunTriggerTest, SunriseLandingOnElevationFiresOnNextPoll) {
  this->expect_landing_fires_next_poll(true, MORNING);
}

TEST_F(SunTriggerTest, SunsetLandingOnElevationFiresOnNextPoll) {
  this->expect_landing_fires_next_poll(false, EVENING);
}

TEST_F(SunTriggerTest, NanElevationKeepsPreviousElevation) {
  auto &sunrise = this->add(true, this->elevation_at(MORNING));

  this->tick(MORNING - MINUTE);
  this->sun_.set_latitude(NAN);
  ASSERT_TRUE(std::isnan(this->elevation_at(MORNING)));
  this->tick(MORNING);
  EXPECT_EQ(sunrise.count, 0);

  this->sun_.set_latitude(LATITUDE);
  this->tick(MORNING + MINUTE);
  EXPECT_EQ(sunrise.count, 1);
}

TEST_F(SunTriggerTest, SunsetIgnoresRisingSun) { this->expect_other_direction_ignored(false, MORNING); }

TEST_F(SunTriggerTest, SunriseIgnoresSettingSun) { this->expect_other_direction_ignored(true, EVENING); }

}  // namespace esphome::sun::testing
