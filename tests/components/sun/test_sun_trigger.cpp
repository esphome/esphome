#include <gtest/gtest.h>

#include <cmath>
#include <ctime>
#include <list>

#include "esphome/components/sun/sun.h"

namespace esphome::sun::testing {

// 2024-06-21 00:00:00 UTC; at the Eiffel Tower the sun rises around 03:47 and sets around 19:58 UTC.
static constexpr time_t DAY_START = 1718928000;
static constexpr time_t MINUTE = 60;
static constexpr time_t NOON = DAY_START + 12 * 3600;
static constexpr double LATITUDE = 48.8584;
static constexpr double LONGITUDE = 2.2945;
static constexpr double DEFAULT_ELEVATION = -0.83333;

class FakeClock : public time::RealTimeClock {
 public:
  time_t timestamp_now() override { return this->now; }
  void update() override {}
  time_t now{DAY_START};
};

class RecordAction : public Action<> {
 public:
  explicit RecordAction(const time_t *now) : now_(now) {}
  int count{0};
  time_t last_fired{0};

 protected:
  void play() override {
    this->count++;
    this->last_fired = *this->now_;
  }
  const time_t *now_;
};

struct Watch {
  Automation<> automation;
  RecordAction action;
  explicit Watch(const time_t *now) : action(now) { this->automation.add_action(&this->action); }
};

class SunTriggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sun_.set_time(&this->clock_);
    this->sun_.set_latitude(LATITUDE);
    this->sun_.set_longitude(LONGITUDE);
  }

  // Registers the callback the way codegen does for on_sunrise / on_sunset.
  RecordAction &add_(bool sunrise, double elevation) {
    auto &watch = this->watches_.emplace_back(&this->clock_.now);
    this->poller_.add_on_elevation_callback([&watch, sunrise, elevation](double last, double current) {
      if (sunrise ? rose_past(last, current, elevation) : set_past(last, current, elevation))
        watch.automation.trigger();
    });
    return watch.action;
  }

  // One 60 s poll at time t.
  void tick_(time_t t) {
    this->clock_.now = t;
    this->poller_.update();
  }

  double elevation_at_(time_t t) {
    this->clock_.now = t;
    return this->sun_.elevation();
  }

  FakeClock clock_;
  Sun sun_;
  SunElevationPoller poller_{&this->sun_};
  std::list<Watch> watches_;
};

TEST_F(SunTriggerTest, FullDayFiresEachTriggerOnce) {
  auto &sunrise = this->add_(true, DEFAULT_ELEVATION);
  auto &sunrise_high = this->add_(true, 5.0);
  auto &sunset = this->add_(false, DEFAULT_ELEVATION);
  auto &sunset_high = this->add_(false, 5.0);

  for (time_t t = DAY_START; t < DAY_START + 24 * 3600; t += MINUTE)
    this->tick_(t);

  EXPECT_EQ(sunrise.count, 1);
  EXPECT_EQ(sunrise_high.count, 1);
  EXPECT_EQ(sunset.count, 1);
  EXPECT_EQ(sunset_high.count, 1);
  EXPECT_LT(sunrise.last_fired, sunrise_high.last_fired);
  EXPECT_LT(sunrise_high.last_fired, NOON);
  EXPECT_GT(sunset_high.last_fired, NOON);
  EXPECT_LT(sunset_high.last_fired, sunset.last_fired);

  // Each fired on the first poll past its elevation.
  EXPECT_LE(this->elevation_at_(sunrise.last_fired - MINUTE), DEFAULT_ELEVATION);
  EXPECT_GT(this->elevation_at_(sunrise.last_fired), DEFAULT_ELEVATION);
  EXPECT_GE(this->elevation_at_(sunset.last_fired - MINUTE), DEFAULT_ELEVATION);
  EXPECT_LT(this->elevation_at_(sunset.last_fired), DEFAULT_ELEVATION);
}

TEST_F(SunTriggerTest, FirstPollNeverFires) {
  const time_t start = NOON - 3 * 3600;
  const double current = this->elevation_at_(start);
  // Already past both thresholds on the first poll.
  auto &sunrise = this->add_(true, current - 1.0);
  auto &sunset = this->add_(false, current + 1.0);

  this->tick_(start);
  EXPECT_EQ(sunrise.count, 0);
  EXPECT_EQ(sunset.count, 0);

  // The morning keeps rising: no new crossing for either.
  this->tick_(start + MINUTE);
  EXPECT_EQ(sunrise.count, 0);
  EXPECT_EQ(sunset.count, 0);
}

TEST_F(SunTriggerTest, SunriseLandingOnElevationFiresOnNextPoll) {
  const time_t t1 = DAY_START + 5 * 3600;
  auto &sunrise = this->add_(true, this->elevation_at_(t1));

  this->tick_(t1 - MINUTE);
  this->tick_(t1);
  EXPECT_EQ(sunrise.count, 0);
  this->tick_(t1 + MINUTE);
  EXPECT_EQ(sunrise.count, 1);
  EXPECT_EQ(sunrise.last_fired, t1 + MINUTE);
}

TEST_F(SunTriggerTest, SunsetLandingOnElevationFiresOnNextPoll) {
  const time_t t1 = DAY_START + 19 * 3600;
  auto &sunset = this->add_(false, this->elevation_at_(t1));

  this->tick_(t1 - MINUTE);
  this->tick_(t1);
  EXPECT_EQ(sunset.count, 0);
  this->tick_(t1 + MINUTE);
  EXPECT_EQ(sunset.count, 1);
  EXPECT_EQ(sunset.last_fired, t1 + MINUTE);
}

TEST_F(SunTriggerTest, NanElevationKeepsPreviousElevation) {
  const time_t t1 = DAY_START + 5 * 3600;
  auto &sunrise = this->add_(true, this->elevation_at_(t1));

  this->tick_(t1 - MINUTE);
  this->sun_.set_latitude(NAN);
  ASSERT_TRUE(std::isnan(this->elevation_at_(t1)));
  this->tick_(t1);
  EXPECT_EQ(sunrise.count, 0);

  this->sun_.set_latitude(LATITUDE);
  this->tick_(t1 + MINUTE);
  EXPECT_EQ(sunrise.count, 1);
}

TEST_F(SunTriggerTest, SunsetIgnoresRisingSun) {
  const time_t morning = DAY_START + 5 * 3600;
  auto &sunset = this->add_(false, this->elevation_at_(morning));

  this->tick_(morning - MINUTE);
  this->tick_(morning + MINUTE);
  EXPECT_EQ(sunset.count, 0);
}

TEST_F(SunTriggerTest, SunriseIgnoresSettingSun) {
  const time_t evening = DAY_START + 19 * 3600;
  auto &sunrise = this->add_(true, this->elevation_at_(evening));

  this->tick_(evening - MINUTE);
  this->tick_(evening + MINUTE);
  EXPECT_EQ(sunrise.count, 0);
}

}  // namespace esphome::sun::testing
