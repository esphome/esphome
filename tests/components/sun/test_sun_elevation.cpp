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
static constexpr time_t UNSYNCED = 0;
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
  Automation<> automation;
  LambdaAction<> action;
  explicit Watch(std::function<void()> &&on_fire) : action(std::move(on_fire)) {
    this->automation.add_action(&this->action);
  }
};

class SunElevationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sun_.set_time(&this->clock_);
    this->sun_.set_latitude(LATITUDE);
    this->sun_.set_longitude(LONGITUDE);
  }

  // Adds the automation the way codegen does for on_sunrise / on_sunset.
  Record &add_(bool sunrise, double elevation) {
    auto &record = this->records_.emplace_back();
    auto &watch = this->watches_.emplace_back([this, &record]() {
      record.count++;
      record.last_fired = this->clock_.now;
    });
    if (sunrise) {
      this->sun_.add_sunrise_automation(&watch.automation, static_cast<float>(elevation));
    } else {
      this->sun_.add_sunset_automation(&watch.automation, static_cast<float>(elevation));
    }
    return record;
  }

  // One 60 s poll at time t.
  void tick_(time_t t) {
    this->clock_.now = t;
    this->sun_.poll_elevation();
  }

  double elevation_at_(time_t t) {
    this->clock_.now = t;
    return this->sun_.elevation();
  }

  // The elevation is stored as a float, so the sun passes it between t - 1 min and t + 1 min.
  void expect_fires_on_first_poll_past_(bool sunrise, time_t t) {
    const float elevation = static_cast<float>(this->elevation_at_(t));
    auto &record = this->add_(sunrise, elevation);
    this->tick_(t - MINUTE);
    this->tick_(t);
    this->tick_(t + MINUTE);
    ASSERT_EQ(record.count, 1);
    const double before = this->elevation_at_(record.last_fired - MINUTE);
    const double after = this->elevation_at_(record.last_fired);
    EXPECT_TRUE(sunrise ? before <= elevation : before >= elevation);
    EXPECT_TRUE(sunrise ? after > elevation : after < elevation);
  }

  // The sun passes the elevation in the other direction around t.
  void expect_other_direction_ignored_(bool sunrise, time_t t) {
    auto &record = this->add_(sunrise, this->elevation_at_(t));
    this->tick_(t - MINUTE);
    this->tick_(t + MINUTE);
    EXPECT_EQ(record.count, 0);
  }

  FakeClock clock_;
  Sun sun_;
  std::list<Record> records_;
  std::list<Watch> watches_;
};

TEST_F(SunElevationTest, FullDayFiresEachTriggerOnce) {
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

TEST_F(SunElevationTest, FirstPollNeverFires) {
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

TEST_F(SunElevationTest, SunriseFiresOnFirstPollPast) { this->expect_fires_on_first_poll_past_(true, MORNING); }

TEST_F(SunElevationTest, SunsetFiresOnFirstPollPast) { this->expect_fires_on_first_poll_past_(false, EVENING); }

// Landing exactly on the elevation does not fire; leaving it in the right direction does.
TEST(SunCrossing, RosePastEdges) {
  EXPECT_FALSE(rose_past(4.0, 5.0, 5.0));
  EXPECT_TRUE(rose_past(5.0, 6.0, 5.0));
  EXPECT_TRUE(rose_past(4.0, 6.0, 5.0));
  EXPECT_FALSE(rose_past(6.0, 4.0, 5.0));
  EXPECT_FALSE(rose_past(NAN, 6.0, 5.0));
}

TEST(SunCrossing, SetPastEdges) {
  EXPECT_FALSE(set_past(6.0, 5.0, 5.0));
  EXPECT_TRUE(set_past(5.0, 4.0, 5.0));
  EXPECT_TRUE(set_past(6.0, 4.0, 5.0));
  EXPECT_FALSE(set_past(4.0, 6.0, 5.0));
  EXPECT_FALSE(set_past(NAN, 4.0, 5.0));
}

TEST_F(SunElevationTest, NanElevationKeepsPreviousElevation) {
  auto &sunrise = this->add_(true, this->elevation_at_(MORNING));

  this->tick_(MORNING - MINUTE);
  // A clock that has not synced reads the epoch, which is not a valid time.
  ASSERT_TRUE(std::isnan(this->elevation_at_(UNSYNCED)));
  this->tick_(UNSYNCED);
  EXPECT_EQ(sunrise.count, 0);

  this->tick_(MORNING + MINUTE);
  EXPECT_EQ(sunrise.count, 1);
}

TEST_F(SunElevationTest, SunsetIgnoresRisingSun) { this->expect_other_direction_ignored_(false, MORNING); }

TEST_F(SunElevationTest, SunriseIgnoresSettingSun) { this->expect_other_direction_ignored_(true, EVENING); }

}  // namespace esphome::sun::testing
