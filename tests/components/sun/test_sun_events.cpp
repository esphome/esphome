#include <gtest/gtest.h>

#include <ctime>

#include "esphome/components/sun/sun.h"

namespace esphome::sun::testing {
namespace {

constexpr time_t HOUR = 3600;
constexpr time_t DAY = 24 * HOUR;
// 2024-06-21 00:00:00 UTC and 2024-12-21 00:00:00 UTC
constexpr time_t JUNE_SOLSTICE = 1718928000;
constexpr time_t DECEMBER_SOLSTICE = 1734739200;
constexpr double DEFAULT_ELEVATION = -0.83333;
// Reference times from the NOAA algorithm; the implementation targets about one minute.
constexpr time_t TOLERANCE = 90;

class FakeClock : public time::RealTimeClock {
 public:
  time_t timestamp_now() override { return this->now; }
  void update() override {}
  time_t now{0};
};

struct Site {
  const char *name;
  double latitude;
  double longitude;
};

constexpr Site PARIS{"paris", 48.8584, 2.2945};
constexpr Site AUCKLAND{"auckland", -36.8485, 174.7633};
constexpr Site HONOLULU{"honolulu", 21.3069, -157.8583};
constexpr Site TROMSO{"tromso", 69.6496, 18.9560};
constexpr Site SITES[] = {
    PARIS,
    AUCKLAND,
    HONOLULU,
    {"sydney", -33.8688, 151.2093},
    {"quito", -0.1807, -78.4678},
    {"anchorage", 61.2181, -149.9003},
    {"tokyo", 35.6762, 139.6503},
};

class SunEventsTest : public ::testing::Test {
 protected:
  void at_(const Site &site, time_t now) {
    this->clock_.now = now;
    this->sun_.set_time(&this->clock_);
    this->sun_.set_latitude(site.latitude);
    this->sun_.set_longitude(site.longitude);
  }

  FakeClock clock_;
  Sun sun_;
};

TEST_F(SunEventsTest, ParisSolstice) {
  this->at_(PARIS, JUNE_SOLSTICE);
  auto rise = this->sun_.sunrise(DEFAULT_ELEVATION);
  auto set = this->sun_.sunset(DEFAULT_ELEVATION);
  ASSERT_TRUE(rise.has_value());
  ASSERT_TRUE(set.has_value());
  EXPECT_NEAR(rise->timestamp, 1718941655, TOLERANCE);  // 03:47:35 UTC
  EXPECT_NEAR(set->timestamp, 1718999872, TOLERANCE);   // 19:57:52 UTC
}

// Far east, the sunrise of a UTC date happens on the previous UTC day. Late in the UTC day the next
// sunrise belongs to the date after tomorrow.
TEST_F(SunEventsTest, NextSunriseFarEastLateInUtcDay) {
  this->at_(AUCKLAND, JUNE_SOLSTICE + 21 * HOUR);
  auto rise = this->sun_.sunrise(DEFAULT_ELEVATION);
  ASSERT_TRUE(rise.has_value());
  EXPECT_NEAR(rise->timestamp, 1719084864, TOLERANCE);  // 2024-06-22 19:34:24 UTC
}

// Far west, the sunset of a UTC date happens on the next UTC day. Early in the UTC day the next
// sunset belongs to yesterday's date.
TEST_F(SunEventsTest, NextSunsetFarWestEarlyInUtcDay) {
  this->at_(HONOLULU, JUNE_SOLSTICE);
  auto set = this->sun_.sunset(DEFAULT_ELEVATION);
  ASSERT_TRUE(set.has_value());
  EXPECT_NEAR(set->timestamp, 1718946959, TOLERANCE);  // 2024-06-21 05:15:59 UTC
}

TEST_F(SunEventsTest, NextEventIsNeverInThePastNorMoreThanADayAway) {
  for (const auto &site : SITES) {
    for (time_t day : {JUNE_SOLSTICE, DECEMBER_SOLSTICE}) {
      for (int hour = 0; hour < 24; hour += 3) {
        const time_t now = day + hour * HOUR;
        this->at_(site, now);
        for (bool rising : {true, false}) {
          auto event = rising ? this->sun_.sunrise(DEFAULT_ELEVATION) : this->sun_.sunset(DEFAULT_ELEVATION);
          ASSERT_TRUE(event.has_value()) << site.name << " hour " << hour << " rising " << rising;
          EXPECT_GE(event->timestamp, now) << site.name << " hour " << hour << " rising " << rising;
          EXPECT_LT(event->timestamp, now + DAY + HOUR) << site.name << " hour " << hour << " rising " << rising;
        }
      }
    }
  }
}

TEST_F(SunEventsTest, PolarDayAndNightHaveNoEvents) {
  for (time_t day : {JUNE_SOLSTICE, DECEMBER_SOLSTICE}) {
    this->at_(TROMSO, day + 12 * HOUR);
    EXPECT_FALSE(this->sun_.sunrise(DEFAULT_ELEVATION).has_value());
    EXPECT_FALSE(this->sun_.sunset(DEFAULT_ELEVATION).has_value());
  }
}

}  // namespace
}  // namespace esphome::sun::testing
